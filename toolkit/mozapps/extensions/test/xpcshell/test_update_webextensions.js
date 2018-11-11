"use strict";

const TOOLKIT_ID = "toolkit@mozilla.org";

// We don't have an easy way to serve update manifests from a secure URL.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

var testserver = createHttpServer();
gPort = testserver.identity.primaryPort;

const uuidGenerator = AM_Cc["@mozilla.org/uuid-generator;1"].getService(AM_Ci.nsIUUIDGenerator);

const extensionsDir = gProfD.clone();
extensionsDir.append("extensions");

const addonsDir = gTmpD.clone();
addonsDir.append("addons");
addonsDir.create(AM_Ci.nsIFile.DIRECTORY_TYPE, 0o755);

do_register_cleanup(() => addonsDir.remove(true));

testserver.registerDirectory("/addons/", addonsDir);


let gUpdateManifests = {};

function mapManifest(aPath, aManifestData) {
  gUpdateManifests[aPath] = aManifestData;
  testserver.registerPathHandler(aPath, serveManifest);
}

function serveManifest(request, response) {
  let manifest = gUpdateManifests[request.path];

  response.setHeader("Content-Type", manifest.contentType, false);
  response.write(manifest.data);
}


function promiseInstallWebExtension(aData) {
  let addonFile = createTempWebExtensionFile(aData);

  return promiseInstallAllFiles([addonFile]).then(() => {
    Services.obs.notifyObservers(addonFile, "flush-cache-entry", null);
    return promiseAddonByID(aData.id);
  });
}

var checkUpdates = Task.async(function* (aData, aReason = AddonManager.UPDATE_WHEN_PERIODIC_UPDATE) {
  function provide(obj, path, value) {
    path = path.split(".");
    let prop = path.pop();

    for (let key of path) {
      if (!(key in obj))
          obj[key] = {};
      obj = obj[key];
    }

    if (!(prop in obj))
      obj[prop] = value;
  }

  let id = uuidGenerator.generateUUID().number;
  provide(aData, "addon.id", id);
  provide(aData, "addon.manifest.applications.gecko.id", id);

  let updatePath = `/updates/${id}.json`.replace(/[{}]/g, "");
  let updateUrl = `http://localhost:${gPort}${updatePath}`

  let addonData = { updates: [] };
  let manifestJSON = {
    addons: { [id]: addonData }
  };


  provide(aData, "addon.manifest.applications.gecko.update_url", updateUrl);
  let awaitInstall = promiseInstallWebExtension(aData.addon);


  for (let version of Object.keys(aData.updates)) {
    let update = aData.updates[version];
    update.version = version;

    provide(update, "addon.id", id);
    provide(update, "addon.manifest.applications.gecko.id", id);
    let addon = update.addon;

    delete update.addon;

    let file;
    if (addon.rdf) {
      provide(addon, "version", version);
      provide(addon, "targetApplications", [{id: TOOLKIT_ID,
                                             minVersion: "42",
                                             maxVersion: "*"}]);

      file = createTempXPIFile(addon);
    } else {
      provide(addon, "manifest.version", version);
      file = createTempWebExtensionFile(addon);
    }
    file.moveTo(addonsDir, `${id}-${version}.xpi`.replace(/[{}]/g, ""));

    let path = `/addons/${file.leafName}`;
    provide(update, "update_link", `http://localhost:${gPort}${path}`);

    addonData.updates.push(update);
  }

  mapManifest(updatePath, { data: JSON.stringify(manifestJSON),
                            contentType: "application/json" });


  let addon = yield awaitInstall;

  let updates = yield promiseFindAddonUpdates(addon, aReason);
  updates.addon = addon;

  return updates;
});


function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "42.0", "42.0");

  startupManager();
  do_register_cleanup(promiseShutdownManager);

  run_next_test();
}


// Check that compatibility updates are applied.
add_task(function* checkUpdateMetadata() {
  let update = yield checkUpdates({
    addon: {
      manifest: {
        version: "1.0",
        applications: { gecko: { strict_max_version: "45" } },
      }
    },
    updates: {
      "1.0": {
        applications: { gecko: { strict_min_version: "40",
                                 strict_max_version: "48" } },
      }
    }
  });

  ok(update.compatibilityUpdate, "have compat update");
  ok(!update.updateAvailable, "have no add-on update");

  ok(update.addon.isCompatibleWith("40", "40"), "compatible min");
  ok(update.addon.isCompatibleWith("48", "48"), "compatible max");
  ok(!update.addon.isCompatibleWith("49", "49"), "not compatible max");

  update.addon.uninstall();
});


// Check that updates from web extensions to web extensions succeed.
add_task(function* checkUpdateToWebExt() {
  let update = yield checkUpdates({
    addon: { manifest: { version: "1.0" } },
    updates: {
      "1.1": { },
      "1.2": { },
      "1.3": { "applications": { "gecko": { "strict_min_version": "48" } } },
    }
  });

  ok(!update.compatibilityUpdate, "have no compat update");
  ok(update.updateAvailable, "have add-on update");

  equal(update.addon.version, "1.0", "add-on version");

  yield promiseCompleteAllInstalls([update.updateAvailable]);

  let addon = yield promiseAddonByID(update.addon.id);
  equal(addon.version, "1.2", "new add-on version");

  addon.uninstall();
});


// Check that updates from web extensions to XUL extensions fail.
add_task(function* checkUpdateToRDF() {
  let update = yield checkUpdates({
    addon: { manifest: { version: "1.0" } },
    updates: {
      "1.1": { addon: { rdf: true } },
    }
  });

  ok(!update.compatibilityUpdate, "have no compat update");
  ok(update.updateAvailable, "have add-on update");

  equal(update.addon.version, "1.0", "add-on version");

  let result = yield new Promise((resolve, reject) => {
    update.updateAvailable.addListener({
      onDownloadFailed: resolve,
      onDownloadEnded: reject,
      onInstalling: reject,
      onInstallStarted: reject,
      onInstallEnded: reject,
    });
    update.updateAvailable.install();
  });

  equal(result.error, AddonManager.ERROR_UNEXPECTED_ADDON_TYPE, "error: unexpected add-on type");

  let addon = yield promiseAddonByID(update.addon.id);
  equal(addon.version, "1.0", "new add-on version");

  addon.uninstall();
});


// Check that illegal update URLs are rejected.
add_task(function* checkIllegalUpdateURL() {
  const URLS = ["chrome://browser/content/",
                "data:text/json,...",
                "javascript:;",
                "/"];

  for (let url of URLS) {
    let { messages } = yield promiseConsoleOutput(() => {
      return new Promise((resolve, reject) => {
        let addonFile = createTempWebExtensionFile({
          manifest: { applications: { gecko: { update_url: url } } },
        });

        AddonManager.getInstallForFile(addonFile, install => {
          Services.obs.notifyObservers(addonFile, "flush-cache-entry", null);

          if (install && install.state == AddonManager.STATE_DOWNLOAD_FAILED)
            resolve();
          reject(new Error("Unexpected state: " + (install && install.state)))
        });
      });
    });

    ok(messages.some(msg => /Access denied for URL|may not load or link to|is not a valid URL/.test(msg)),
       "Got checkLoadURI error");
  }
});
