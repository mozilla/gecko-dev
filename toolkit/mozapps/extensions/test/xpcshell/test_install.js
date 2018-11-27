/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testserver = createHttpServer({hosts: ["example.com"]});
var gInstallDate;

const ADDONS = {
  test_install1: {
    manifest: {
      name: "Test 1",
      version: "1.0",
      applications: {gecko: {id: "addon1@tests.mozilla.org"}},
    },
  },
  test_install2_1: {
    manifest: {
      name: "Test 2",
      version: "2.0",
      applications: {gecko: {id: "addon2@tests.mozilla.org"}},
    },
  },
  test_install2_2: {
    manifest: {
      name: "Test 2",
      version: "3.0",
      applications: {gecko: {id: "addon2@tests.mozilla.org"}},
    },
  },
  test_install3: {
    manifest: {
      name: "Test 3",
      version: "1.0",
      applications: {
        gecko: {
          id: "addon3@tests.mozilla.org",
          strict_min_version: "0",
          strict_max_version: "0",
          update_url: "http://example.com/update.json",
        },
      },
    },
  },
};

const XPIS = {};

// The test extension uses an insecure update url.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);
Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);

const profileDir = gProfD.clone();
profileDir.append("extensions");

const UPDATE_JSON = {
  addons: {
    "addon3@tests.mozilla.org": {
      updates: [
        {
          version: "1.0",
          applications: {
            gecko: {
              strict_min_version: "0",
              strict_max_version: "2",
            },
          },
        },
      ],
    },
  },
};

const GETADDONS_JSON = {
  page_size: 25,
  page_count: 1,
  count: 1,
  next: null,
  previous: null,
  results: [
    {
      name: "Test 2",
      type: "extension",
      guid: "addon2@tests.mozilla.org",
      current_version: {
        version: "1.0",
        files: [
          {
            size: 2,
            url: "http://example.com/test_install2_1.xpi",
          },
        ],
      },
      authors: [
        {
          name: "Test Creator",
          url: "http://example.com/creator.html",
        },
      ],
      summary: "Repository summary",
      description: "Repository description",
    },
  ],
};

const COMPAT_JSON = {
  page_size: 25,
  page_count: 1,
  count: 0,
  next: null,
  previous: null,
  results: [ ],
};

function checkInstall(install, expected) {
  for (let [key, value] of Object.entries(expected)) {
    if (value instanceof Ci.nsIURI) {
      equal(install[key] && install[key].spec, value.spec, `Expected value of install.${key}`);
    } else {
      deepEqual(install[key], value, `Expected value of install.${key}`);
    }
  }
}

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  for (let [name, data] of Object.entries(ADDONS)) {
    XPIS[name] = AddonTestUtils.createTempWebExtensionFile(data);
    testserver.registerFile(`/addons/${name}.xpi`, XPIS[name]);
  }

  await promiseStartupManager();

  // Create and configure the HTTP server.
  AddonTestUtils.registerJSON(testserver, "/update.json", UPDATE_JSON);
  testserver.registerDirectory("/data/", do_get_file("data"));
  testserver.registerPathHandler("/redirect", function(aRequest, aResponse) {
    aResponse.setStatusLine(null, 301, "Moved Permanently");
    let url = aRequest.host + ":" + aRequest.port + aRequest.queryString;
    aResponse.setHeader("Location", "http://" + url);
  });
  gPort = testserver.identity.primaryPort;
});

// Checks that an install from a local file proceeds as expected
add_task(async function test_install_file() {
  let [, install] = await Promise.all([
    AddonTestUtils.promiseInstallEvent("onNewInstall"),
    AddonManager.getInstallForFile(XPIS.test_install1),
  ]);

  let uri = Services.io.newFileURI(XPIS.test_install1);
  checkInstall(install, {
    type: "extension",
    version: "1.0",
    name: "Test 1",
    state: AddonManager.STATE_DOWNLOADED,
    sourceURI: uri,
  });

  let {addon} = install;
  checkAddon("addon1@tests.mozilla.org", addon, {
    install,
    sourceURI: uri,
  });
  notEqual(addon.syncGUID, null);
  equal(addon.getResourceURI("manifest.json").spec, `jar:${uri.spec}!/manifest.json`);

  let activeInstalls = await AddonManager.getAllInstalls();
  equal(activeInstalls.length, 1);
  equal(activeInstalls[0], install);

  let fooInstalls = await AddonManager.getInstallsByTypes(["foo"]);
  equal(fooInstalls.length, 0);

  let extensionInstalls = await AddonManager.getInstallsByTypes(["extension"]);
  equal(extensionInstalls.length, 1);
  equal(extensionInstalls[0], install);

  prepare_test({
    "addon1@tests.mozilla.org": [
      ["onInstalling", false],
      "onInstalled",
    ],
  }, [
    "onInstallStarted",
    "onInstallEnded",
  ]);

  await install.install();

  ensure_test_completed();

  addon = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  ok(addon);

  ok(!hasFlag(addon.permissions, AddonManager.PERM_CAN_ENABLE));
  ok(hasFlag(addon.permissions, AddonManager.PERM_CAN_DISABLE));

  let updateDate = Date.now();

  await promiseRestartManager();

  activeInstalls = await AddonManager.getAllInstalls();
  equal(activeInstalls, 0);

  let a1 = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  let uri2 = do_get_addon_root_uri(profileDir, "addon1@tests.mozilla.org");

  checkAddon("addon1@tests.mozilla.org", a1, {
    type: "extension",
    version: "1.0",
    name: "Test 1",
    foreignInstall: false,
    sourceURI: Services.io.newFileURI(XPIS.test_install1),
  });

  notEqual(a1.syncGUID, null);
  ok(a1.syncGUID.length >= 9);

  ok(isExtensionInBootstrappedList(profileDir, a1.id));
  ok(XPIS.test_install1.exists());
  do_check_in_crash_annotation(a1.id, a1.version);

  let difference = a1.installDate.getTime() - updateDate;
  if (Math.abs(difference) > MAX_TIME_DIFFERENCE)
    do_throw("Add-on install time was out by " + difference + "ms");

  difference = a1.updateDate.getTime() - updateDate;
  if (Math.abs(difference) > MAX_TIME_DIFFERENCE)
    do_throw("Add-on update time was out by " + difference + "ms");

  equal(a1.getResourceURI("manifest.json").spec, uri2 + "manifest.json");

  // Ensure that extension bundle (or icon if unpacked) has updated
  // lastModifiedDate.
  let testURI = a1.getResourceURI("");
  let testFile = testURI.QueryInterface(Ci.nsIFileURL).file;
  ok(testFile.exists());
  difference = testFile.lastModifiedTime - Date.now();
  ok(Math.abs(difference) < MAX_TIME_DIFFERENCE);

  await a1.uninstall();
  let { id, version } = a1;
  await promiseRestartManager();
  do_check_not_in_crash_annotation(id, version);
});

// Tests that an install from a url downloads.
add_task(async function test_install_url() {
  let url = "http://example.com/addons/test_install2_1.xpi";
  let install = await AddonManager.getInstallForURL(url, "application/x-xpinstall", null, "Test 2", null, "1.0");
  checkInstall(install, {
    version: "1.0",
    name: "Test 2",
    state: AddonManager.STATE_AVAILABLE,
    sourceURI: Services.io.newURI(url),
  });

  let activeInstalls = await AddonManager.getAllInstalls();
  equal(activeInstalls.length, 1);
  equal(activeInstalls[0], install);

  await new Promise(resolve => {
    prepare_test({}, [
      "onDownloadStarted",
      "onDownloadEnded",
    ], () => {
      resolve();
      return false;
    });
    install.install();
  });

  ensure_test_completed();

  checkInstall(install, {
    version: "2.0",
    name: "Test 2",
    state: AddonManager.STATE_DOWNLOADED,
  });
  equal(install.addon.install, install);

  prepare_test({
    "addon2@tests.mozilla.org": [
      ["onInstalling", false],
      "onInstalled",
    ],
  }, [
    "onInstallStarted",
    "onInstallEnded",
  ]);

  await install.install();
  ensure_test_completed();

  let updateDate = Date.now();

  await promiseRestartManager();

  let installs = await AddonManager.getAllInstalls();
  equal(installs, 0);

  let a2 = await AddonManager.getAddonByID("addon2@tests.mozilla.org");
  checkAddon("addon2@tests.mozilla.org", a2, {
    type: "extension",
    version: "2.0",
    name: "Test 2",
    sourceURI: Services.io.newURI(url),
  });
  notEqual(a2.syncGUID, null);

  ok(isExtensionInBootstrappedList(profileDir, a2.id));
  ok(XPIS.test_install2_1.exists());
  do_check_in_crash_annotation(a2.id, a2.version);

  let difference = a2.installDate.getTime() - updateDate;
  Assert.lessOrEqual(Math.abs(difference), MAX_TIME_DIFFERENCE,
                     "Add-on install time was correct");

  difference = a2.updateDate.getTime() - updateDate;
  Assert.lessOrEqual(Math.abs(difference), MAX_TIME_DIFFERENCE,
                     "Add-on update time was correct");

  gInstallDate = a2.installDate;
});

// Tests that installing a new version of an existing add-on works
add_task(async function test_install_new_version() {
  let url = "http://example.com/addons/test_install2_2.xpi";
  let [, install] = await Promise.all([
    AddonTestUtils.promiseInstallEvent("onNewInstall"),
    AddonManager.getInstallForURL(url, "application/x-xpinstall", null, "Test 2", null, "3.0"),
  ]);

  checkInstall(install, {
    version: "3.0",
    name: "Test 2",
    state: AddonManager.STATE_AVAILABLE,
    existingAddon: null,
  });

  let activeInstalls = await AddonManager.getAllInstalls();
  equal(activeInstalls.length, 1);
  equal(activeInstalls[0], install);

  await new Promise(resolve => {
    prepare_test({}, [
      "onDownloadStarted",
      "onDownloadEnded",
    ], () => {
      resolve();
      return false;
    });
    install.install();
  });

  ensure_test_completed();

  checkInstall(install, {
    version: "3.0",
    name: "Test 2",
    state: AddonManager.STATE_DOWNLOADED,
    existingAddon: await AddonManager.getAddonByID("addon2@tests.mozilla.org"),
  });

  equal(install.addon.install, install);

  // Installation will continue when there is nothing returned.
  prepare_test({
    "addon2@tests.mozilla.org": [
      ["onInstalling", false],
      "onInstalled",
    ],
  }, [
    "onInstallStarted",
    "onInstallEnded",
  ]);

  await install.install();

  ensure_test_completed();

  await promiseRestartManager();

  let installs2 = await AddonManager.getInstallsByTypes(null);
  equal(installs2.length, 0);

  let a2 = await AddonManager.getAddonByID("addon2@tests.mozilla.org");
  checkAddon("addon2@tests.mozilla.org", a2, {
    type: "extension",
    version: "3.0",
    name: "Test 2",
    isActive: true,
    foreignInstall: false,
    sourceURI: Services.io.newURI(url),
    installDate: gInstallDate,
  });

  ok(isExtensionInBootstrappedList(profileDir, a2.id));
  ok(XPIS.test_install2_2.exists());
  do_check_in_crash_annotation(a2.id, a2.version);

  // Update date should be later (or the same if this test is too fast)
  ok(a2.installDate <= a2.updateDate);

  await a2.uninstall();
});

// Tests that an install that requires a compatibility update works
add_task(async function test_install_compat_update() {
  let url = "http://example.com/addons/test_install3.xpi";
  let [, install] = await Promise.all([
    AddonTestUtils.promiseInstallEvent("onNewInstall"),
    await AddonManager.getInstallForURL(url, "application/x-xpinstall", null, "Test 3", null, "1.0"),
  ]);

  checkInstall(install, {
    version: "1.0",
    name: "Test 3",
    state: AddonManager.STATE_AVAILABLE,
  });

  let activeInstalls = await AddonManager.getInstallsByTypes(null);
  equal(activeInstalls.length, 1);
  equal(activeInstalls[0], install);

  await new Promise(resolve => {
    prepare_test({}, [
      "onDownloadStarted",
      "onDownloadEnded",
    ], () => {
      resolve();
      return false;
    });
    install.install();
  });
  ensure_test_completed();

  checkInstall(install, {
    version: "1.0",
    name: "Test 3",
    state: AddonManager.STATE_DOWNLOADED,
    existingAddon: null,
  });
  checkAddon("addon3@tests.mozilla.org", install.addon, {
    appDisabled: false,
  });

  // Continue the install
  prepare_test({
    "addon3@tests.mozilla.org": [
      ["onInstalling", false],
      "onInstalled",
    ],
  }, [
    "onInstallStarted",
    "onInstallEnded",
  ]);

  await install.install();
  ensure_test_completed();

  await promiseRestartManager();

  let installs = await AddonManager.getAllInstalls();
  equal(installs, 0);

  let a3 = await AddonManager.getAddonByID("addon3@tests.mozilla.org");
  checkAddon("addon3@tests.mozilla.org", a3, {
    type: "extension",
    version: "1.0",
    name: "Test 3",
    isActive: true,
    appDisabled: false,
  });
  notEqual(a3.syncGUID, null);

  ok(isExtensionInBootstrappedList(profileDir, a3.id));

  ok(XPIS.test_install3.exists());
  await a3.uninstall();
});

add_task(async function test_compat_update_local() {
  let [, install] = await Promise.all([
    AddonTestUtils.promiseInstallEvent("onNewInstall"),
    AddonManager.getInstallForFile(XPIS.test_install3),
  ]);
  ok(install.addon.isCompatible);

  prepare_test({
    "addon3@tests.mozilla.org": [
      ["onInstalling", false],
      "onInstalled",
    ],
  }, [
    "onInstallStarted",
    "onInstallEnded",
  ]);

  await install.install();
  ensure_test_completed();

  await promiseRestartManager();

  let a3 = await AddonManager.getAddonByID("addon3@tests.mozilla.org");
  checkAddon("addon3@tests.mozilla.org", a3, {
    type: "extension",
    version: "1.0",
    name: "Test 3",
    isActive: true,
    appDisabled: false,
  });
  notEqual(a3.syncGUID, null);

  ok(isExtensionInBootstrappedList(profileDir, a3.id));

  ok(XPIS.test_install3.exists());
  await a3.uninstall();
});

// Test that after cancelling a download it is removed from the active installs
add_task(async function test_cancel() {
  let url = "http://example.com/addons/test_install3.xpi";
  let [, install] = await Promise.all([
    AddonTestUtils.promiseInstallEvent("onNewInstall"),
    AddonManager.getInstallForURL(url, "application/x-xpinstall", null, "Test 3", null, "1.0"),
  ]);

  checkInstall(install, {
    version: "1.0",
    name: "Test 3",
    state: AddonManager.STATE_AVAILABLE,
  });

  let activeInstalls = await AddonManager.getInstallsByTypes(null);
  equal(activeInstalls.length, 1);
  equal(activeInstalls[0], install);

  await new Promise(resolve => {
    prepare_test({}, [
      "onDownloadStarted",
      "onDownloadEnded",
    ], () => {
      prepare_test({}, [
        "onDownloadCancelled",
      ], resolve);

      install.cancel();
    });

    install.install();
  });
  ensure_test_completed();

  let file = install.file;

  // Allow the file removal to complete
  activeInstalls = await AddonManager.getAllInstalls();
  equal(activeInstalls.length, 0);
  ok(!file.exists());
});

// Check that cancelling the install from onDownloadStarted actually cancels it
add_task(async function test_cancel_onDownloadStarted() {
  clearListeners();
  let url = "http://example.com/addons/test_install2_1.xpi";
  let [, install] = await Promise.all([
    AddonTestUtils.promiseInstallEvent("onNewInstall"),
    AddonManager.getInstallForURL(url, "application/x-xpinstall"),
  ]);

  equal(install.file, null);

  install.addListener({
    onDownloadStarted() {
      install.removeListener(this);
      executeSoon(() => install.cancel());
    },
  });


  let promise = AddonTestUtils.promiseInstallEvent("onDownloadCancelled");
  install.install();
  await promise;

  // Wait another tick to see if it continues downloading.
  // The listener only really tests if we give it time to see progress, the
  // file check isn't ideal either
  install.addListener({
    onDownloadProgress() {
      do_throw("Download should not have continued");
    },
    onDownloadEnded() {
      do_throw("Download should not have continued");
    },
  });

  let file = install.file;
  await Promise.resolve();
  ok(!file.exists());
});

// Checks that cancelling the install from onDownloadEnded actually cancels it
add_task(async function test_cancel_onDownloadEnded() {
  let url = "http://example.com/addons/test_install2_1.xpi";
  let [, install] = await Promise.all([
    AddonTestUtils.promiseInstallEvent("onNewInstall"),
    AddonManager.getInstallForURL(url, "application/x-xpinstall"),
  ]);

  equal(install.file, null);

  await new Promise(resolve => {
    prepare_test({ }, [
      "onDownloadStarted",
      "onDownloadEnded",
    ], () => {
      prepare_test({ }, [
        "onDownloadCancelled",
      ]);

      install.cancel();
      resolve();
    });
    install.install();
  });

  ensure_test_completed();

  install.addListener({
    onInstallStarted() {
      do_throw("Install should not have continued");
    },
  });
});

// Verify that the userDisabled value carries over to the upgrade by default
add_task(async function test_userDisabled_update() {
  clearListeners();
  let url = "http://example.com/addons/test_install2_1.xpi";
  let [, install] = await Promise.all([
    AddonTestUtils.promiseInstallEvent("onNewInstall"),
    AddonManager.getInstallForURL(url, "application/x-xpinstall"),
  ]);

  await install.install();

  ok(!install.addon.userDisabled);
  await install.addon.disable();

  let addon = await AddonManager.getAddonByID("addon2@tests.mozilla.org");
  checkAddon("addon2@tests.mozilla.org", addon, {
    userDisabled: true,
    isActive: false,
  });

  url = "http://example.com/addons/test_install2_2.xpi";
  install = await AddonManager.getInstallForURL(url, "application/x-xpinstall");
  await install.install();

  checkAddon("addon2@tests.mozilla.org", install.addon, {
    userDisabled: true,
    isActive: false,
  });

  await promiseRestartManager();

  addon = await AddonManager.getAddonByID("addon2@tests.mozilla.org");
  checkAddon("addon2@tests.mozilla.org", addon, {
    userDisabled: true,
    isActive: false,
  });

  await addon.uninstall();
});

// Verify that changing the userDisabled value before onInstallEnded works
add_task(async function test_userDisabled() {
  let url = "http://example.com/addons/test_install2_1.xpi";
  let install = await AddonManager.getInstallForURL(url, "application/x-xpinstall");
  await install.install();

  ok(!install.addon.userDisabled);

  let addon = await AddonManager.getAddonByID("addon2@tests.mozilla.org");
  checkAddon("addon2@tests.mozilla.org", addon, {
    userDisabled: false,
    isActive: true,
  });

  url = "http://example.com/addons/test_install2_2.xpi";
  install = await AddonManager.getInstallForURL(url, "application/x-xpinstall");

  install.addListener({
    onInstallStarted() {
      ok(!install.addon.userDisabled);
      install.addon.disable();
    },
  });

  await install.install();

  addon = await AddonManager.getAddonByID("addon2@tests.mozilla.org");
  checkAddon("addon2@tests.mozilla.org", addon, {
    userDisabled: true,
    isActive: false,
  });

  await addon.uninstall();
});

// Checks that metadata is not stored if the pref is set to false
add_task(async function test_18_1() {
  AddonTestUtils.registerJSON(testserver, "/getaddons.json", GETADDONS_JSON);
  Services.prefs.setCharPref(PREF_GETADDONS_BYIDS,
                             "http://example.com/getaddons.json");

  AddonTestUtils.registerJSON(testserver, "/compat.json", COMPAT_JSON);
  Services.prefs.setCharPref(PREF_COMPAT_OVERRIDES,
                             "http://example.com/compat.json");

  Services.prefs.setBoolPref("extensions.getAddons.cache.enabled", true);
  Services.prefs.setBoolPref("extensions.addon2@tests.mozilla.org.getAddons.cache.enabled", false);

  let url = "http://example.com/addons/test_install2_1.xpi";
  let install = await AddonManager.getInstallForURL(url, "application/x-xpinstall");
  await install.install();

  notEqual(install.addon.fullDescription, "Repository description");

  await promiseRestartManager();

  let addon = await AddonManager.getAddonByID("addon2@tests.mozilla.org");
  notEqual(addon.fullDescription, "Repository description");

  await addon.uninstall();
});

// Checks that metadata is downloaded for new installs and is visible before and
// after restart
add_task(async function test_metadata() {
  Services.prefs.setBoolPref("extensions.addon2@tests.mozilla.org.getAddons.cache.enabled", true);

  let url = "http://example.com/addons/test_install2_1.xpi";
  let install = await AddonManager.getInstallForURL(url, "application/x-xpinstall");
  await install.install();

  equal(install.addon.fullDescription, "Repository description");

  await promiseRestartManager();

  let addon = await AddonManager.getAddonByID("addon2@tests.mozilla.org");
  equal(addon.fullDescription, "Repository description");

  await addon.uninstall();
});

// Do the same again to make sure it works when the data is already in the cache
add_task(async function test_metadata_again() {
  let url = "http://example.com/addons/test_install2_1.xpi";
  let install = await AddonManager.getInstallForURL(url, "application/x-xpinstall");
  await install.install();

  equal(install.addon.fullDescription, "Repository description");

  await promiseRestartManager();

  let addon = await AddonManager.getAddonByID("addon2@tests.mozilla.org");
  equal(addon.fullDescription, "Repository description");

  await addon.uninstall();
});

// Tests that an install can be restarted after being cancelled
add_task(async function test_restart() {
  let url = "http://example.com/addons/test_install1.xpi";
  let install = await AddonManager.getInstallForURL(url, "application/x-xpinstall");
  equal(install.state, AddonManager.STATE_AVAILABLE);

  install.addListener({
    onDownloadEnded() {
      install.removeListener(this);
      install.cancel();
    },
  });

  try {
    await install.install();
    ok(false, "Install should not have succeeded");
  } catch (err) { }

  let promise = new Promise(resolve => {
    prepare_test({
      "addon1@tests.mozilla.org": [
        ["onInstalling", false],
        "onInstalled",
      ],
    }, [
      "onDownloadStarted",
      "onDownloadEnded",
      "onInstallStarted",
      "onInstallEnded",
    ], resolve);
    install.install();
  });

  await Promise.all([
    promise,
    promiseWebExtensionStartup("addon1@tests.mozilla.org"),
  ]);
  ensure_test_completed();

  clearListeners();

  await install.addon.uninstall();
});

// Tests that an install can be restarted after being cancelled when a hash
// was provided
add_task(async function test_restart_hash() {
  let url = "http://example.com/addons/test_install1.xpi";
  let install = await AddonManager.getInstallForURL(url, "application/x-xpinstall", do_get_file_hash(XPIS.test_install1));
  equal(install.state, AddonManager.STATE_AVAILABLE);

  install.addListener({
    onDownloadEnded() {
      install.removeListener(this);
      install.cancel();
    },
  });

  try {
    await install.install();
    ok(false, "Install should not have succeeded");
  } catch (err) { }

  let promise = new Promise(resolve => {
    prepare_test({
      "addon1@tests.mozilla.org": [
        ["onInstalling", false],
        "onInstalled",
      ],
    }, [
      "onDownloadStarted",
      "onDownloadEnded",
      "onInstallStarted",
      "onInstallEnded",
    ], resolve);
    install.install();
  });

  await Promise.all([
    promise,
    promiseWebExtensionStartup("addon1@tests.mozilla.org"),
  ]);
  ensure_test_completed();

  clearListeners();

  await install.addon.uninstall();

});

// Tests that an install with a bad hash can be restarted after it fails, though
// it will only fail again
add_task(async function test_restart_badhash() {
  let url = "http://example.com/addons/test_install1.xpi";
  let install = await AddonManager.getInstallForURL(url, "application/x-xpinstall", "sha1:foo");
  equal(install.state, AddonManager.STATE_AVAILABLE);

  install.addListener({
    onDownloadEnded() {
      install.removeListener(this);
      install.cancel();
    },
  });

  try {
    await install.install();
    ok(false, "Install should not have succeeded");
  } catch (err) { }

  try {
    await install.install();
    ok(false, "Install should not have succeeded");
  } catch (err) {
    ok(true, "Resumed install should have failed");
  }
});

// Tests that installs with a hash for a local file work
add_task(async function test_local_hash() {
  let url = Services.io.newFileURI(XPIS.test_install1).spec;
  let install = await AddonManager.getInstallForURL(url, "application/x-xpinstall", do_get_file_hash(XPIS.test_install1));

  checkInstall(install, {
    state: AddonManager.STATE_DOWNLOADED,
    error: 0,
  });

  install.cancel();
});

// Test that an install may be canceled after a redirect.
add_task(async function test_cancel_redirect() {
  let url = "http://example.com/redirect?/addons/test_install1.xpi";
  let install = await AddonManager.getInstallForURL(url, "application/x-xpinstall");

  install.addListener({
    onDownloadProgress() {
      install.cancel();
    },
  });

  let promise = AddonTestUtils.promiseInstallEvent("onDownloadCancelled");

  install.install();
  await promise;

  equal(install.state, AddonManager.STATE_CANCELLED);
});

// Tests that an install can be restarted during onDownloadCancelled after being
// cancelled in mid-download
add_task(async function test_restart2() {
  let url = "http://example.com/addons/test_install1.xpi";
  let install = await AddonManager.getInstallForURL(url, "application/x-xpinstall");

  equal(install.state, AddonManager.STATE_AVAILABLE);

  install.addListener({
    onDownloadProgress() {
      install.removeListener(this);
      install.cancel();
    },
  });

  let promise = AddonTestUtils.promiseInstallEvent("onDownloadCancelled");
  install.install();
  await promise;

  equal(install.state, AddonManager.STATE_CANCELLED);

  promise = new Promise(resolve => {
    prepare_test({
      "addon1@tests.mozilla.org": [
        ["onInstalling", false],
        "onInstalled",
      ],
    }, [
      "onDownloadStarted",
      "onDownloadEnded",
      "onInstallStarted",
      "onInstallEnded",
    ], resolve);

    let file = install.file;
    install.install();
    notEqual(file.path, install.file.path);
    ok(!file.exists());
  });

  await Promise.all([
    promise,
    promiseWebExtensionStartup("addon1@tests.mozilla.org"),
  ]);

  ensure_test_completed();

  clearListeners();
  await install.addon.uninstall();
});
