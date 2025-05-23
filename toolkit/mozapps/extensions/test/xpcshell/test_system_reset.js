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
async function setupOverrideBuiltinsApp1() {
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
async function setupOverrideBuiltinsApp2() {
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
async function setupOverrideBuiltinsApp3() {
  const builtins = [
    await getSystemBuiltin(1, "1.0", "app3-builtin-system1"),
    await getSystemBuiltin(3, "1.0", "app3-builtin-system3"),
    // The following two entries are expected to not be found.
    {
      addon_id: "system2@tests.mozilla.org",
      addon_version: "1.0",
      res_url: `resource://app3-builtin-system2/`,
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
    info(`check_installed: verifying addon ${id}`);
    let addon = await promiseAddonByID(id);

    if (!("isUpgrade" in condition) || !("version" in condition)) {
      throw Error("condition must contain isUpgrade and version");
    }
    let isUpgrade = conditions[i].isUpgrade;
    let version = conditions[i].version;

    const { builtins } =
      AddonTestUtils.getXPIExports().XPIProvider.builtInAddons;
    const foundAsBuiltIn = builtins?.find(entry => entry.addon_id === id);

    if (version) {
      // Add-on should be installed
      Assert.notEqual(addon, null, "add-on should be installed");
      Assert.equal(addon.version, version, "addon.version");
      Assert.ok(addon.isActive, "addon.isActive");
      Assert.ok(!addon.foreignInstall, "!addon.foreignInstall");
      Assert.ok(addon.hidden, "addon.hidden");
      Assert.ok(addon.isSystem, "addon.isSystem");
      Assert.ok(
        !hasFlag(addon.permissions, AddonManager.PERM_CAN_UPGRADE),
        "should not have PERM_CAN_UPGRADE"
      );
      if (isUpgrade) {
        Assert.ok(
          hasFlag(addon.permissions, AddonManager.PERM_API_CAN_UNINSTALL),
          "system-signed update should have PERM_API_CAN_UNINSTALL"
        );
      } else {
        Assert.ok(
          !hasFlag(addon.permissions, AddonManager.PERM_API_CAN_UNINSTALL),
          "auto-installed built-in add-ons update should not have PERM_API_CAN_UNINSTALL"
        );
      }

      // Verify that the add-on file is in the right place
      if (!isUpgrade) {
        Assert.equal(addon.getResourceURI("").spec, foundAsBuiltIn.res_url);
      } else {
        let file = updatesDir.clone();
        file.append(id + ".xpi");
        Assert.ok(file.exists());
        Assert.ok(file.isFile());
        Assert.equal(getAddonFile(addon).path, file.path);
        Assert.equal(
          addon.signedState,
          AddonManager.SIGNEDSTATE_SYSTEM,
          "should be system-signed"
        );
      }
    } else if (isUpgrade) {
      // Add-on should not be installed
      Assert.equal(addon, null, "add-on should not be installed");
    } else {
      // Either add-on should not be installed or it shouldn't be active
      Assert.ok(
        !addon || !addon.isActive,
        "add-on should disabled or not installed"
      );
    }
  }
}

// Test with a missing features directory or system builtin bundled
// assets.
async function test_default_missing() {
  let overrideBuiltInsData = {
    builtins: SYSTEM_DEFAULTS_ADDON_IDS.map(id => {
      return {
        addon_id: id,
        addon_version: "1.0",
        res_url: `resource://missing-builtin-${id.split("@")[0]}/`,
      };
    }),
  };

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
async function test_new_version() {
  gAppInfo.version = "1";

  await setupOverrideBuiltinsApp1();
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
async function test_upgrade() {
  gAppInfo.version = "2";
  await setupOverrideBuiltinsApp2();
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
async function test_downgrade() {
  gAppInfo.version = "1";
  await setupOverrideBuiltinsApp1();
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
async function test_updated() {
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

  await setupOverrideBuiltinsApp1();
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
async function safe_mode_disabled() {
  gAppInfo.inSafeMode = true;

  await setupOverrideBuiltinsApp1();
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
async function normal_mode_enabled() {
  gAppInfo.inSafeMode = false;

  await setupOverrideBuiltinsApp1();
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
async function test_skips_additional() {
  // Copy in the system add-ons
  let file = await getSystemAddonXPI(4, "1.0");
  file.copyTo(updatesDir, "system4@tests.mozilla.org.xpi");

  await setupOverrideBuiltinsApp1();
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
async function test_no_hide_location_on_missing_addon() {
  manuallyUninstall(updatesDir, "system2@tests.mozilla.org");

  await setupOverrideBuiltinsApp1();
  await promiseStartupManager();

  // With system add-on 2 gone the updated set is now invalid so it reverts to
  // the default set which is system add-ons 1 and 2.
  let conditions = [
    { isUpgrade: false, version: "1.0" },
    { isUpgrade: false, version: "1.0" },
    { isUpgrade: true, version: "2.0" },
  ];

  await check_installed(conditions);

  await promiseShutdownManager();
}

// Putting it back will make the set work again
async function test_reuse() {
  let file = await getSystemAddonXPI(2, "2.0");
  file.copyTo(updatesDir, "system2@tests.mozilla.org.xpi");

  await setupOverrideBuiltinsApp1();
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
async function test_corrupt_pref() {
  Services.prefs.setCharPref(PREF_SYSTEM_ADDON_SET, "foo");

  await setupOverrideBuiltinsApp1();
  await promiseStartupManager();

  let conditions = [
    { isUpgrade: false, version: "1.0" },
    { isUpgrade: false, version: "1.0" },
    { isUpgrade: false, version: null },
  ];

  await check_installed(conditions);

  await promiseShutdownManager();
}

// An add-on that is not system-signed in the system-addons profile location should be reset selectively,
// while the rest of the set is expected to be preserved.
async function test_bad_profile_cert() {
  AddonTestUtils.usePrivilegedSignatures = id => {
    return id === "system1@tests.mozilla.org" ? false : "system";
  };

  let file = await getSystemAddonXPI(1, "2.0");
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
        version: "2.0",
      },
      "system3@tests.mozilla.org": {
        version: "2.0",
      },
    },
  };
  Services.prefs.setCharPref(PREF_SYSTEM_ADDON_SET, JSON.stringify(addonSet));

  await setupOverrideBuiltinsApp1();

  const { messages } = await AddonTestUtils.promiseConsoleOutput(async () => {
    await promiseStartupManager();
  });

  // Expected condition
  let conditions = [
    { isUpgrade: false, version: "1.0" },
    { isUpgrade: true, version: "2.0" },
    { isUpgrade: true, version: "2.0" },
  ];

  await check_installed(conditions);

  // Verify that the system1 add-on from the system-addons location was disabled
  // because it was not correctly signed.
  const expectedMessage = /system1@tests.mozilla.org is not correctly signed/;
  AddonTestUtils.checkMessages(messages, {
    expected: [{ message: expectedMessage }],
  });

  // system1 version got from Balrog was not system-signed and so it has been disabled
  // (and the underlying builtin add-on version revealed), but it is still expected to
  // be listed in the extesions.systemAddonSet pref.
  verifySystemAddonSetPref({
    "system1@tests.mozilla.org": {
      version: "2.0",
    },
    "system2@tests.mozilla.org": {
      version: "2.0",
    },
    "system3@tests.mozilla.org": {
      version: "2.0",
    },
  });

  await promiseShutdownManager();

  AddonTestUtils.usePrivilegedSignatures = () => "system";
}

// Built-in addons installed in the system-builds location are bundled into the omni jar
// and so the buil-in addon version is expected to never be disabled due to an unexpected
// signed state and should still work.
async function test_system_signature_is_not_required_for_builtins() {
  gAppInfo.version = "3";

  AddonTestUtils.usePrivilegedSignatures = id => {
    return id === "system1@tests.mozilla.org" ? false : "system";
  };

  await setupOverrideBuiltinsApp3();
  await promiseStartupManager();

  // Since we updated the app version, the system addons set got for the previous app
  // version is compared with the new set of builtin add-ons versions and the system-signed
  // add-ons are kept if the app has a matching builtin add-ons with an older version
  // (on the contrary system-signed xpi files are uninstalled if older then the builtin
  // addon version or if there isn't a matching built-in addon).
  verifySystemAddonSetPref({
    "system1@tests.mozilla.org": {
      version: "2.0",
    },
    "system2@tests.mozilla.org": {
      version: "2.0",
    },
    "system3@tests.mozilla.org": {
      version: "2.0",
    },
  });

  // Add-on will still be present
  let addon = await promiseAddonByID("system1@tests.mozilla.org");
  Assert.notEqual(addon, null);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  let conditions = [
    { isUpgrade: false, version: "1.0" },
    { isUpgrade: true, version: "2.0" },
    { isUpgrade: true, version: "2.0" },
  ];

  await check_installed(conditions);

  // system1 version got from Balrog was not system-signed and so it has been disabled
  // (and the underlying builtin add-on version revealed), but it is still expected to
  // be listed in the extesions.systemAddonSet pref.
  verifySystemAddonSetPref({
    "system1@tests.mozilla.org": {
      version: "2.0",
    },
    "system2@tests.mozilla.org": {
      version: "2.0",
    },
    "system3@tests.mozilla.org": {
      version: "2.0",
    },
  });

  await promiseShutdownManager();

  AddonTestUtils.usePrivilegedSignatures = () => "system";
}

// A failed upgrade should revert to the default set.
async function test_updated_bad_update_set() {
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

  await setupOverrideBuiltinsApp3();
  await promiseStartupManager();

  let conditions = [{ isUpgrade: false, version: "1.0" }];

  await check_installed(conditions);

  await promiseShutdownManager();
}

add_task(async function run_system_reset_scenarios() {
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
    test_no_hide_location_on_missing_addon,
    test_reuse,
    test_corrupt_pref,
    test_bad_profile_cert,
    test_system_signature_is_not_required_for_builtins,
    test_updated_bad_update_set,
  ];
  for (const test_fn of test_scenarios) {
    info(`===== Entering test scenario: ${test_fn.name} =====`);
    await test_fn();
    info(`===== Exiting test scenario: ${test_fn.name} =====`);
  }
});
