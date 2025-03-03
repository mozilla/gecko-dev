// Tests that we reset to the default system add-ons correctly when switching
// application versions

let updatesDir = FileUtils.getDir("ProfD", ["features"]);

AddonTestUtils.usePrivilegedSignatures = () => "system";

// Enable SCOPE_APPLICATION for builtin testing.  Default in tests is only SCOPE_PROFILE.
let scopes = AddonManager.SCOPE_PROFILE | AddonManager.SCOPE_APPLICATION;
Services.prefs.setIntPref("extensions.enabledScopes", scopes);

const distroDir = FileUtils.getDir("ProfD", ["sysfeatures", "app0"]);
distroDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
registerDirectory("XREAppFeat", distroDir);

const SYSTEM_DEFAULTS_ADDON_IDS = [
  "system1@tests.mozilla.org",
  "system2@tests.mozilla.org",
  "system3@tests.mozilla.org",
  "system5@tests.mozilla.org",
];

// Setup app1 pre-installed system addons: system1 1.0, system2 1.0, system3/system5 missing
async function setupOverrideBuiltinsApp1({ asBuiltIn = true }) {
  if (!asBuiltIn) {
    let dir = FileUtils.getDir("ProfD", ["sysfeatures", "app1"]);
    if (!dir.exists()) {
      dir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
      let xpi = await getSystemAddonXPI(1, "1.0");
      xpi.copyTo(dir, "system1@tests.mozilla.org.xpi");
      xpi = await getSystemAddonXPI(2, "1.0");
      xpi.copyTo(dir, "system2@tests.mozilla.org.xpi");
    }
    distroDir.leafName = "app1";
    await overrideBuiltIns({ system: SYSTEM_DEFAULTS_ADDON_IDS });
    return;
  }

  const builtins = [
    await getSystemBuiltin(1, "1.0", "app1-builtin-system1"),
    await getSystemBuiltin(2, "1.0", "app1-builtin-system2"),
    // The following two entries are expected to be missing,
    // and so they are expected to not be installed while not
    // preventing the other entries to be installed correctly.
    {
      addon_id: "system3@tests.mozilla.org",
      addon_version: "1.0",
      res_url: `resource://app1-builtin-system3/`,
    },
    {
      addon_id: "system5@tests.mozilla.org",
      addon_version: "1.0",
      res_url: `resource://app1-builtin-system5/`,
    },
  ];
  await overrideBuiltIns({ builtins });
}

// Setup app2 pre-installed system addons: system1 2.0, system3 1.0, system2/system5 missing
async function setupOverrideBuiltinsApp2({ asBuiltIn = true }) {
  if (!asBuiltIn) {
    let dir = FileUtils.getDir("ProfD", ["sysfeatures", "app2"]);
    if (!dir.exists()) {
      dir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
      let xpi = await getSystemAddonXPI(1, "2.0");
      xpi.copyTo(dir, "system1@tests.mozilla.org.xpi");
      xpi = await getSystemAddonXPI(3, "1.0");
      xpi.copyTo(dir, "system3@tests.mozilla.org.xpi");
    }
    distroDir.leafName = "app2";
    await overrideBuiltIns({ system: SYSTEM_DEFAULTS_ADDON_IDS });
    return;
  }

  const builtins = [
    await getSystemBuiltin(1, "2.0", "app2-builtin-system1"),
    await getSystemBuiltin(3, "1.0", "app2-builtin-system3"),
    // The following two entries are expected to not be found.
    {
      addon_id: "system2@tests.mozilla.org",
      addon_version: "1.0",
      res_url: `resource://app2-builtin-system2/`,
    },
    {
      addon_id: "system5@tests.mozilla.org",
      addon_version: "1.0",
      res_url: `resource://app2-builtin-system5/`,
    },
  ];
  await overrideBuiltIns({ builtins });
}

// setup app3 pre-installed system addons: system1 1.0, system3 1.0, system2/system5 missing
async function setupOverrideBuiltinsApp3({ asBuiltIn = true }) {
  if (!asBuiltIn) {
    let dir = FileUtils.getDir("ProfD", ["sysfeatures", "app3"]);
    if (!dir.exists()) {
      dir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
      let xpi = await getSystemAddonXPI(1, "1.0");
      xpi.copyTo(dir, "system1@tests.mozilla.org.xpi");
      xpi = await getSystemAddonXPI(3, "1.0");
      xpi.copyTo(dir, "system3@tests.mozilla.org.xpi");
    }
    distroDir.leafName = "app3";
    await overrideBuiltIns({ system: SYSTEM_DEFAULTS_ADDON_IDS });
    return;
  }

  const builtins = [
    await getSystemBuiltin(1, "1.0", "app3-builtin-system1"),
    await getSystemBuiltin(3, "1.0", "app3-builtin-system3"),
    // The following two entries are expected to not be found.
    {
      addon_id: "system2@tests.mozilla.org",
      addon_version: "1.0",
      res_url: `resource://app3-builtin-system3/`,
    },
    {
      addon_id: "system5@tests.mozilla.org",
      addon_version: "1.0",
      res_url: `resource://app3-builtin-system5/`,
    },
  ];
  await overrideBuiltIns({ builtins });
}

function makeUUID() {
  let uuidGen = Services.uuid;
  return uuidGen.generateUUID().toString();
}

async function check_installed(conditions) {
  for (let i = 0; i < conditions.length; i++) {
    let condition = conditions[i];
    let id = "system" + (i + 1) + "@tests.mozilla.org";
    let addon = await promiseAddonByID(id);

    if (!("isUpgrade" in condition) || !("version" in condition)) {
      throw Error("condition must contain isUpgrade and version");
    }
    let isUpgrade = conditions[i].isUpgrade;
    let version = conditions[i].version;

    const { builtins } =
      AddonTestUtils.getXPIExports().XPIProvider.builtInAddons;
    const foundAsBuiltIn = builtins?.find(entry => entry.addon_id === id);

    let expectedDir = isUpgrade ? updatesDir : distroDir;

    if (version) {
      // Add-on should be installed
      Assert.notEqual(addon, null);
      Assert.equal(addon.version, version);
      Assert.ok(addon.isActive);
      Assert.ok(!addon.foreignInstall);
      Assert.ok(addon.hidden);
      Assert.ok(addon.isSystem);
      Assert.ok(!hasFlag(addon.permissions, AddonManager.PERM_CAN_UPGRADE));
      if (isUpgrade) {
        Assert.ok(
          hasFlag(addon.permissions, AddonManager.PERM_API_CAN_UNINSTALL)
        );
      } else {
        Assert.ok(
          !hasFlag(addon.permissions, AddonManager.PERM_API_CAN_UNINSTALL)
        );
      }

      // Verify that the add-on file is in the right place
      if (!!foundAsBuiltIn && !isUpgrade) {
        Assert.equal(addon.getResourceURI("").spec, foundAsBuiltIn.res_url);
      } else {
        let file = expectedDir.clone();
        file.append(id + ".xpi");
        Assert.ok(file.exists());
        Assert.ok(file.isFile());
        Assert.equal(getAddonFile(addon).path, file.path);
      }

      if (isUpgrade) {
        Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_SYSTEM);
      }
    } else if (isUpgrade) {
      // Add-on should not be installed
      Assert.equal(addon, null);
    } else {
      // Either add-on should not be installed or it shouldn't be active
      Assert.ok(!addon || !addon.isActive);
    }
  }
}

// Test with a missing features directory or system builtin bundled
// assets.
async function test_default_missing({ asBuiltIn = true } = {}) {
  let overrideBuiltInsData = {
    builtins: SYSTEM_DEFAULTS_ADDON_IDS.map(id => {
      return {
        addon_id: id,
        addon_version: "1.0",
        res_url: `resource://missing-builtin-${id.split("@")[0]}/`,
      };
    }),
  };

  if (!asBuiltIn) {
    overrideBuiltInsData = {
      system: SYSTEM_DEFAULTS_ADDON_IDS,
    };
  }

  await overrideBuiltIns(overrideBuiltInsData);
  await promiseStartupManager();

  let conditions = [
    { isUpgrade: false, version: null },
    { isUpgrade: false, version: null },
    { isUpgrade: false, version: null },
  ];

  await check_installed(conditions);

  Assert.ok(!updatesDir.exists());

  await promiseShutdownManager();
}

// Add some features in a new version
async function test_new_version({ asBuiltIn = true } = {}) {
  gAppInfo.version = "1";

  await setupOverrideBuiltinsApp1({ asBuiltIn });
  await promiseStartupManager();

  let conditions = [
    { isUpgrade: false, version: "1.0" },
    { isUpgrade: false, version: "1.0" },
    { isUpgrade: false, version: null },
  ];

  await check_installed(conditions);

  Assert.ok(!updatesDir.exists());

  await promiseShutdownManager();
}

// Another new version swaps one feature and upgrades another
async function test_upgrade({ asBuiltIn = true } = {}) {
  gAppInfo.version = "2";
  await setupOverrideBuiltinsApp2({ asBuiltIn });
  await promiseStartupManager();

  let conditions = [
    { isUpgrade: false, version: "2.0" },
    { isUpgrade: false, version: null },
    { isUpgrade: false, version: "1.0" },
  ];

  await check_installed(conditions);

  Assert.ok(!updatesDir.exists());

  await promiseShutdownManager();
}

// Downgrade
async function test_downgrade({ asBuiltIn = true } = {}) {
  gAppInfo.version = "1";
  await setupOverrideBuiltinsApp1({ asBuiltIn });
  await promiseStartupManager();

  let conditions = [
    { isUpgrade: false, version: "1.0" },
    { isUpgrade: false, version: "1.0" },
    { isUpgrade: false, version: null },
  ];

  await check_installed(conditions);

  Assert.ok(!updatesDir.exists());

  await promiseShutdownManager();
}

// Fake a mid-cycle install
async function test_updated({ asBuiltIn = true } = {}) {
  // Create a random dir to install into
  let dirname = makeUUID();
  let dir = FileUtils.getDir("ProfD", ["features", dirname]);
  dir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  updatesDir = dir;

  // Copy in the system add-ons
  let file = await getSystemAddonXPI(2, "2.0");
  file.copyTo(updatesDir, "system2@tests.mozilla.org.xpi");
  file = await getSystemAddonXPI(3, "2.0");
  file.copyTo(updatesDir, "system3@tests.mozilla.org.xpi");

  // Inject it into the system set
  let addonSet = {
    schema: 1,
    directory: updatesDir.leafName,
    addons: {
      "system2@tests.mozilla.org": {
        version: "2.0",
      },
      "system3@tests.mozilla.org": {
        version: "2.0",
      },
    },
  };
  Services.prefs.setCharPref(PREF_SYSTEM_ADDON_SET, JSON.stringify(addonSet));

  await setupOverrideBuiltinsApp1({ asBuiltIn });
  await promiseStartupManager();

  let conditions = [
    { isUpgrade: false, version: "1.0" },
    { isUpgrade: true, version: "2.0" },
    { isUpgrade: true, version: "2.0" },
  ];

  await check_installed(conditions);

  await promiseShutdownManager();
}

// Entering safe mode should disable the updated system add-ons and use the
// default system add-ons
async function safe_mode_disabled({ asBuiltIn = true } = {}) {
  gAppInfo.inSafeMode = true;

  await setupOverrideBuiltinsApp1({ asBuiltIn });
  await promiseStartupManager();

  let conditions = [
    { isUpgrade: false, version: "1.0" },
    { isUpgrade: false, version: "1.0" },
    { isUpgrade: false, version: null },
  ];

  await check_installed(conditions);

  await promiseShutdownManager();
}

// Leaving safe mode should re-enable the updated system add-ons
async function normal_mode_enabled({ asBuiltIn = true } = {}) {
  gAppInfo.inSafeMode = false;

  await setupOverrideBuiltinsApp1({ asBuiltIn });
  await promiseStartupManager();

  let conditions = [
    { isUpgrade: false, version: "1.0" },
    { isUpgrade: true, version: "2.0" },
    { isUpgrade: true, version: "2.0" },
  ];

  await check_installed(conditions);

  await promiseShutdownManager();
}

// An additional add-on in the directory should be ignored
async function test_skips_additional({ asBuiltIn = true } = {}) {
  // Copy in the system add-ons
  let file = await getSystemAddonXPI(4, "1.0");
  file.copyTo(updatesDir, "system4@tests.mozilla.org.xpi");

  await setupOverrideBuiltinsApp1({ asBuiltIn });
  await promiseStartupManager();

  let conditions = [
    { isUpgrade: false, version: "1.0" },
    { isUpgrade: true, version: "2.0" },
    { isUpgrade: true, version: "2.0" },
  ];

  await check_installed(conditions);

  await promiseShutdownManager();
}

// Missing add-on should revert to the default set
async function test_revert({ asBuiltIn = true } = {}) {
  manuallyUninstall(updatesDir, "system2@tests.mozilla.org");

  await setupOverrideBuiltinsApp1({ asBuiltIn });
  await promiseStartupManager();

  // With system add-on 2 gone the updated set is now invalid so it reverts to
  // the default set which is system add-ons 1 and 2.
  let conditions = [
    { isUpgrade: false, version: "1.0" },
    { isUpgrade: false, version: "1.0" },
    { isUpgrade: false, version: null },
  ];

  await check_installed(conditions);

  await promiseShutdownManager();
}

// Putting it back will make the set work again
async function test_reuse({ asBuiltIn = true } = {}) {
  let file = await getSystemAddonXPI(2, "2.0");
  file.copyTo(updatesDir, "system2@tests.mozilla.org.xpi");

  await setupOverrideBuiltinsApp1({ asBuiltIn });
  await promiseStartupManager();

  let conditions = [
    { isUpgrade: false, version: "1.0" },
    { isUpgrade: true, version: "2.0" },
    { isUpgrade: true, version: "2.0" },
  ];

  await check_installed(conditions);

  await promiseShutdownManager();
}

// Making the pref corrupt should revert to the default set
async function test_corrupt_pref({ asBuiltIn = true } = {}) {
  Services.prefs.setCharPref(PREF_SYSTEM_ADDON_SET, "foo");

  await setupOverrideBuiltinsApp1({ asBuiltIn });
  await promiseStartupManager();

  let conditions = [
    { isUpgrade: false, version: "1.0" },
    { isUpgrade: false, version: "1.0" },
    { isUpgrade: false, version: null },
  ];

  await check_installed(conditions);

  await promiseShutdownManager();
}

// An add-on with a bad certificate should cause us to use the default set
async function test_bad_profile_cert({ asBuiltIn = true } = {}) {
  let file = await getSystemAddonXPI(1, "1.0");
  file.copyTo(updatesDir, "system1@tests.mozilla.org.xpi");

  // Inject it into the system set
  let addonSet = {
    schema: 1,
    directory: updatesDir.leafName,
    addons: {
      "system1@tests.mozilla.org": {
        version: "2.0",
      },
      "system2@tests.mozilla.org": {
        version: "1.0",
      },
      "system3@tests.mozilla.org": {
        version: "1.0",
      },
    },
  };
  Services.prefs.setCharPref(PREF_SYSTEM_ADDON_SET, JSON.stringify(addonSet));

  await setupOverrideBuiltinsApp1({ asBuiltIn });
  await promiseStartupManager();

  let conditions = [
    { isUpgrade: false, version: "1.0" },
    { isUpgrade: false, version: "1.0" },
    { isUpgrade: false, version: null },
  ];

  await check_installed(conditions);

  await promiseShutdownManager();
}

// Switching to app defaults that contain a bad certificate should still work
async function test_bad_app_cert({ asBuiltIn = true } = {}) {
  gAppInfo.version = "3";

  AddonTestUtils.usePrivilegedSignatures = id => {
    return id === "system1@tests.mozilla.org" ? false : "system";
  };

  await setupOverrideBuiltinsApp3({ asBuiltIn });
  await promiseStartupManager();

  // Since we updated the app version, the system addon set should be reset as well.
  let addonSet = Services.prefs.getCharPref(PREF_SYSTEM_ADDON_SET);
  Assert.equal(addonSet, `{"schema":1,"addons":{}}`);

  // Add-on will still be present
  let addon = await promiseAddonByID("system1@tests.mozilla.org");
  Assert.notEqual(addon, null);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  let conditions = [
    { isUpgrade: false, version: "1.0" },
    { isUpgrade: false, version: null },
    { isUpgrade: false, version: "1.0" },
  ];

  await check_installed(conditions);

  await promiseShutdownManager();

  AddonTestUtils.usePrivilegedSignatures = () => "system";
}

// A failed upgrade should revert to the default set.
async function test_updated_bad_update_set({ asBuiltIn = true } = {}) {
  // Create a random dir to install into
  let dirname = makeUUID();
  let dir = FileUtils.getDir("ProfD", ["features", dirname]);
  dir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  updatesDir = dir;

  // Copy in the system add-ons
  let file = await getSystemAddonXPI(2, "2.0");
  file.copyTo(updatesDir, "system2@tests.mozilla.org.xpi");
  file = await getSystemAddonXPI("failed_update", "1.0");
  file.copyTo(updatesDir, "system_failed_update@tests.mozilla.org.xpi");

  // Inject it into the system set
  let addonSet = {
    schema: 1,
    directory: updatesDir.leafName,
    addons: {
      "system2@tests.mozilla.org": {
        version: "2.0",
      },
      "system_failed_update@tests.mozilla.org": {
        version: "1.0",
      },
    },
  };
  Services.prefs.setCharPref(PREF_SYSTEM_ADDON_SET, JSON.stringify(addonSet));

  await setupOverrideBuiltinsApp3({ asBuiltIn });
  await promiseStartupManager();

  let conditions = [{ isUpgrade: false, version: "1.0" }];

  await check_installed(conditions);

  await promiseShutdownManager();
}

async function run_system_reset_scenarios({ asBuiltIn = true } = {}) {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "0");
  clearSystemAddonUpdatesDir();
  // NOTE: these test scenarios are not independent from each other,
  // in most cases each test scenario expects to start from the state
  // that the previous test scenario ends with.
  const test_scenarios = [
    test_default_missing,
    test_new_version,
    test_upgrade,
    test_downgrade,
    test_updated,
    safe_mode_disabled,
    normal_mode_enabled,
    test_skips_additional,
    test_revert,
    test_reuse,
    test_corrupt_pref,
    test_bad_profile_cert,
    test_bad_app_cert,
    test_updated_bad_update_set,
  ];
  for (const test_fn of test_scenarios) {
    info(`===== Entering test scenario: ${test_fn.name} =====`);
    await test_fn({ asBuiltIn });
    info(`===== Exiting test scenario: ${test_fn.name} =====`);
  }
}

// TODO(Bug 1949847): remove this test along with removing the app-system-defaults location.
add_task(async function test_with_systemdefault_as_xpi() {
  await run_system_reset_scenarios({ asBuiltIn: false });
});

add_task(async function test_with_systemdefault_as_builtin() {
  await run_system_reset_scenarios();
});
