// Tests that AddonRepository doesn't download results for system add-ons

// Enable SCOPE_APPLICATION for builtin testing.  Default in tests is only SCOPE_PROFILE.
let scopes = AddonManager.SCOPE_PROFILE | AddonManager.SCOPE_APPLICATION;
Services.prefs.setIntPref("extensions.enabledScopes", scopes);

const PREF_GETADDONS_CACHE_ENABLED = "extensions.getAddons.cache.enabled";

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "0");

var gServer = new HttpServer();

add_setup(() => {
  gServer.start(-1);
  gServer.registerPathHandler("/get", () => {
    do_throw("Unexpected request to server.");
  });
  registerCleanupFunction(async () => {
    await new Promise(resolve => gServer.stop(resolve));
  });
});

// Test with a missing features directory
async function test_app_addons({ asBuiltIn = true } = {}) {
  // Build the test set xpis
  if (!asBuiltIn) {
    var distroDir = FileUtils.getDir("ProfD", ["sysfeatures"]);
    distroDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
    let xpi = await getSystemAddonXPI(1, "1.0");
    xpi.copyTo(distroDir, "system1@tests.mozilla.org.xpi");

    xpi = await getSystemAddonXPI(2, "1.0");
    xpi.copyTo(distroDir, "system2@tests.mozilla.org.xpi");

    xpi = await getSystemAddonXPI(3, "1.0");
    xpi.copyTo(distroDir, "system3@tests.mozilla.org.xpi");

    registerDirectory("XREAppFeat", distroDir);
  }

  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);
  Services.prefs.setCharPref(
    PREF_GETADDONS_BYIDS,
    `http://localhost:${gServer.identity.primaryPort}/get?%IDS%`
  );

  await overrideBuiltIns({
    builtins: asBuiltIn
      ? [
          await getSystemBuiltin(1),
          await getSystemBuiltin(2),
          await getSystemBuiltin(3),
        ]
      : [],
    system: asBuiltIn
      ? []
      : [
          "system1@tests.mozilla.org",
          "system2@tests.mozilla.org",
          "system3@tests.mozilla.org",
        ],
  });

  await promiseStartupManager();

  await AddonRepository.cacheAddons([
    "system1@tests.mozilla.org",
    "system2@tests.mozilla.org",
    "system3@tests.mozilla.org",
  ]);

  let cached = await AddonRepository.getCachedAddonByID(
    "system1@tests.mozilla.org"
  );
  Assert.equal(cached, null);

  cached = await AddonRepository.getCachedAddonByID(
    "system2@tests.mozilla.org"
  );
  Assert.equal(cached, null);

  cached = await AddonRepository.getCachedAddonByID(
    "system3@tests.mozilla.org"
  );
  Assert.equal(cached, null);

  await promiseShutdownManager();
}

// TODO(Bug 1949847): remove this test along with removing the app-system-defaults location.
add_task(async function test_app_addons_xpi() {
  await test_app_addons({ asBuiltIn: false });
});

add_task(async function test_app_addons_systembuiltin() {
  await test_app_addons();
});
