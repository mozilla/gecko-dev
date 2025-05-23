/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Additional tests specifically targeting system addons installed from assets bundled in the omni jar.

AddonTestUtils.usePrivilegedSignatures = () => "system";

const appInfoCommon = {
  ID: "xpcshell@tests.mozilla.org",
  name: "XPCShell",
  platformVersion: "1.0",
};
const appInfoInitial = {
  ...appInfoCommon,
  version: "2",
  appBuildID: "appBuildID-1",
  lastAppVersion: "",
  lastAppBuildID: "",
};
const appInfoUpdatedBuildID = {
  ...appInfoCommon,
  version: "2",
  appBuildID: "appBuildID-1",
  lastAppVersion: "2",
  lastAppBuildID: "appBuildID-2",
};
const appInfoUpdatedVersion = {
  ...appInfoCommon,
  version: "3",
  appBuildID: "appBuildID-2",
  lastAppVersion: "3",
  lastAppBuildID: "appBuildID-2",
};

AddonTestUtils.updateAppInfo(appInfoInitial);

// Enable SCOPE_APPLICATION for builtin testing.  Default in tests is only SCOPE_PROFILE.
let scopes = AddonManager.SCOPE_PROFILE | AddonManager.SCOPE_APPLICATION;
Services.prefs.setIntPref("extensions.enabledScopes", scopes);

const PREF_EM_LAST_APP_BUILD_ID = "extensions.lastAppBuildId";

add_setup(initSystemAddonDirs);

async function assertAddonProperties(id, expectedVersion) {
  let addon = await AddonManager.getAddonByID(id);
  Assert.notEqual(addon, null, "Expect addon to be found");
  Assert.equal(addon.version, expectedVersion, "Got expected addon version");
  Assert.ok(addon.isSystem, "Expect addon to be isSystem");
  Assert.ok(addon.isBuiltin, "Expect addon to be isBuiltIn");
  Assert.ok(addon.isActive, "Expect addon to be isActive");
  Assert.ok(addon.hidden, "Expect addon to be hidden");
  Assert.ok(
    !addon.foreignInstall,
    "Expect addon to not have foreignInstall set to true"
  );
}

async function setupSystemBuiltin(id, version, addon_res_url_path) {
  await setupBuiltinExtension(
    {
      manifest: {
        name: `Built-In System Add-on`,
        version,
        browser_specific_settings: {
          gecko: { id },
        },
      },
    },
    addon_res_url_path
  );
  let builtins = [
    {
      addon_id: id,
      addon_version: version,
      res_url: `resource://${addon_res_url_path}/`,
    },
  ];
  await overrideBuiltIns({ builtins });
}

async function assertAOMStartupChanges(ids) {
  const foundIds = await AddonManager.getStartupChanges(
    AddonManager.STARTUP_CHANGE_CHANGED
  );
  Assert.deepEqual(
    foundIds.filter(id => ids.includes(id)).sort(),
    ids.sort(),
    "Got expected addon ids listed in AOM STARTUP_CHANGE_CHANGED"
  );
}

add_task(
  async function test_metadata_updated_on_app_version_and_buildid_changed() {
    // Sanity check.
    ok(
      Services.appinfo.appBuildID,
      "Servics.appinfo.appBuildID should not be empty"
    );

    const id = "system1@tests.mozilla.org";
    await setupSystemBuiltin(id, "1.0", "builtin-ext-path-v1");

    info("Test system builtin addon installed in a new profile");
    await Promise.all([
      promiseStartupManager(),
      promiseWebExtensionStartup(id),
    ]);
    Assert.equal(
      Services.prefs.getCharPref(PREF_EM_LAST_APP_BUILD_ID, ""),
      Services.appinfo.appBuildID,
      "build ID is correct after a startup"
    );
    await assertAddonProperties(id, "1.0");
    // AOM STARTUP_CHANGE_CHANGED is expected to be empty when the profile is brand new (see
    // https://searchfox.org/mozilla-central/rev/488e2d83/toolkit/mozapps/extensions/AddonManager.sys.mjs#758-763)
    assertAOMStartupChanges([]);
    await promiseShutdownManager();

    info(
      "Test system builtin addon version bump in existing profile where last appBuildID != new appBuildID"
    );
    // Ensure changes to builtin system addon are picked up when running on existing profiles and only
    // app build id has changed.
    AddonTestUtils.updateAppInfo(appInfoUpdatedBuildID);
    Services.prefs.setCharPref(PREF_EM_LAST_APP_BUILD_ID, "");
    await setupSystemBuiltin(id, "1.1", "builtin-ext-path-v1-1");
    await Promise.all([
      promiseStartupManager(),
      promiseWebExtensionStartup(id),
    ]);
    await assertAddonProperties(id, "1.1");
    assertAOMStartupChanges([id]);
    // Next startup should not find any other change.
    await Promise.all([
      promiseRestartManager(),
      promiseWebExtensionStartup(id),
    ]);
    await assertAddonProperties(id, "1.1");
    assertAOMStartupChanges([]);
    await promiseShutdownManager();

    // Ensure changes to builtin system addon are picked up when running on existing profiles and
    // app version has changed.
    info(
      "Test system builtin addon version bump in existing profile where last appVersion != new appVersion"
    );
    AddonTestUtils.updateAppInfo(appInfoUpdatedVersion);
    await setupSystemBuiltin(id, "2.0", "builtin-ext-path-v2");
    await Promise.all([
      promiseStartupManager(),
      promiseWebExtensionStartup(id),
    ]);
    await assertAddonProperties(id, "2.0");
    assertAOMStartupChanges([id]);
    // Next startup should not find any other change.
    await Promise.all([
      promiseRestartManager(),
      promiseWebExtensionStartup(id),
    ]);
    await assertAddonProperties(id, "2.0");
    assertAOMStartupChanges([]);
    await promiseShutdownManager();
  }
);

/**
 * Test that on application version upgrades we keep the set of system-signed updates
 * to built-in add-ons, but still reset the entire add-on set got from
 * Balrog on application version downgrades.
 */
add_task(async function test_noreset_system_signed_on_app_upgrade() {
  const idBuiltin = "system-builtin@tests.mozilla.org";
  const idHotfix = "system-hotfix@tests.mozilla.org";
  let addonBuiltin;
  let addonHotfix;

  info("Test system builtin addon initially installed in a new profile");
  AddonTestUtils.updateAppInfo({ version: "1", lastAppVersion: "" });
  await setupSystemBuiltin(idBuiltin, "1.0", "builtin-ext-path-v1");
  await Promise.all([
    promiseStartupManager(),
    promiseWebExtensionStartup(idBuiltin),
  ]);
  await checkAddon(idBuiltin, "1.0", BOOTSTRAP_REASONS.ADDON_INSTALL);
  await assertAddonProperties(idBuiltin, "1.0");
  addonBuiltin = await AddonManager.getAddonByID(idBuiltin);
  Assert.equal(
    addonBuiltin.locationName,
    "app-builtin-addons",
    `got addon ${idBuiltin} in the expected builtin location`
  );

  // Sanity Checks:
  // AOM STARTUP_CHANGE_CHANGED is expected to be empty when the profile is brand new (see
  // https://searchfox.org/mozilla-central/rev/488e2d83/toolkit/mozapps/extensions/AddonManager.sys.mjs#758-763)
  assertAOMStartupChanges([]);
  // PREF_EM_LAST_APP_BUILD_ID is expected to be set.
  Assert.equal(
    Services.prefs.getCharPref(PREF_EM_LAST_APP_BUILD_ID, ""),
    Services.appinfo.appBuildID,
    "build ID is correct after a startup"
  );

  // Mock a new system addon update check
  info("Mock system-signed update installed");
  // Simulated addons set delivered through Balrog:
  // - a system-signed update version 3.0 to builtin addon version 1.0 (idBuiltin)
  // - a system-signed hotfix addon with version 1.0.1 (idHotfix)
  await promiseUpdateSystemAddonsSet([
    { id: idBuiltin, version: "3.0" },
    { id: idHotfix, version: "1.0.1" },
  ]);

  verifySystemAddonSetPref({
    [idBuiltin]: {
      version: "3.0",
    },
    [idHotfix]: {
      version: "1.0.1",
    },
  });

  await checkAddon(idBuiltin, "3.0", BOOTSTRAP_REASONS.ADDON_UPGRADE);
  await assertAddonProperties(idBuiltin, "3.0");
  addonBuiltin = await AddonManager.getAddonByID(idBuiltin);
  Assert.equal(
    addonBuiltin.locationName,
    "app-system-addons",
    `got addon ${idBuiltin} in the system-signed updates location`
  );

  await checkAddon(idHotfix, "1.0.1", BOOTSTRAP_REASONS.ADDON_INSTALL);
  await assertAddonProperties(idHotfix, "1.0.1");
  addonHotfix = await AddonManager.getAddonByID(idHotfix);
  Assert.equal(
    addonHotfix.locationName,
    "app-system-addons",
    `got addon ${idHotfix} in the system-signed updates location`
  );

  await promiseShutdownManager();

  info(
    "Test app version upgrade (should keep system-signed updates and remove hotfix addon)"
  );
  AddonTestUtils.updateAppInfo({ version: "2", lastAppVersion: "1" });
  await setupSystemBuiltin(idBuiltin, "2.0", "builtin-ext-path-v2");
  await Promise.all([
    promiseStartupManager(),
    promiseWebExtensionStartup(idBuiltin),
  ]);
  await assertAddonProperties(idBuiltin, "3.0");
  addonBuiltin = await AddonManager.getAddonByID(idBuiltin);
  Assert.equal(
    addonBuiltin.locationName,
    "app-system-addons",
    `got addon ${idBuiltin} in the system-signed updates location`
  );
  addonHotfix = await AddonManager.getAddonByID(idHotfix);
  Assert.equal(addonHotfix, null, `addon ${idHotfix} is not found as expected`);

  verifySystemAddonSetPref({
    [idBuiltin]: {
      version: "3.0",
    },
  });

  info(
    "Test Uninstalling the system addon update (should reveal the underlying builtin version)"
  );
  await addonBuiltin.uninstall();
  await assertAddonProperties(idBuiltin, "2.0");
  addonBuiltin = await AddonManager.getAddonByID(idBuiltin);
  Assert.equal(
    addonBuiltin.locationName,
    "app-builtin-addons",
    `got addon ${idBuiltin} in the expected builtin location`
  );

  info("Mock system-signed update installed back");
  await promiseUpdateSystemAddon(idBuiltin, "3.0");
  await checkAddon(idBuiltin, "3.0", BOOTSTRAP_REASONS.ADDON_UPGRADE);
  await promiseShutdownManager();

  info("Test app version downgrade (should reset system-signed updates)");
  AddonTestUtils.updateAppInfo({ version: "1", lastAppVersion: "2" });
  await setupSystemBuiltin(idBuiltin, "1.0", "builtin-ext-path-v1");
  await Promise.all([
    promiseStartupManager(),
    promiseWebExtensionStartup(idBuiltin),
  ]);
  await assertAddonProperties(idBuiltin, "1.0");
  addonBuiltin = await AddonManager.getAddonByID(idBuiltin);
  Assert.equal(
    addonBuiltin.locationName,
    "app-builtin-addons",
    `got addon ${idBuiltin} in the expected builtin location`
  );

  verifySystemAddonSetPref({});

  await promiseShutdownManager();
});

/**
 * Test that on application upgrades we uninstall the system-signed add-ons that
 * are part of the add-ons set previously got from balrog if during application
 * version changes we detect that the system-signed add-on version has a lower
 * version compared to the corresponding add-on built in the omni jar.
 */
add_task(
  async function test_reset_systemsigned_set_on_builtin_version_downgrade() {
    const idBuiltin = "system-builtin@tests.mozilla.org";
    const idHotfix = "system-hotfix@tests.mozilla.org";
    let addonBuiltin;
    let addonHotfix;

    info(
      "Setup initial conditions: app version 1, builtin v1, new system-signed v2)"
    );
    AddonTestUtils.updateAppInfo({ version: "1", lastAppVersion: "" });
    await setupSystemBuiltin(idBuiltin, "1.0", "builtin-ext-path-v1");
    await Promise.all([
      promiseStartupManager(),
      promiseWebExtensionStartup(idBuiltin),
    ]);
    await checkAddon(idBuiltin, "1.0", BOOTSTRAP_REASONS.ADDON_INSTALL);
    await assertAddonProperties(idBuiltin, "1.0");
    addonBuiltin = await AddonManager.getAddonByID(idBuiltin);
    Assert.equal(
      addonBuiltin.locationName,
      "app-builtin-addons",
      `got addon ${idBuiltin} in the expected builtin location`
    );

    // Simulated addons set delivered through Balrog:
    // - a system-signed update version 2.0 to builtin addon version 1.0 (idBuiltin)
    // - a system-signed hotfix addon with version 1.0.1 (idHotfix)
    await promiseUpdateSystemAddonsSet([
      { id: idBuiltin, version: "2.0" },
      { id: idHotfix, version: "1.0.1" },
    ]);

    verifySystemAddonSetPref({
      [idBuiltin]: {
        version: "2.0",
      },
      [idHotfix]: {
        version: "1.0.1",
      },
    });

    await checkAddon(idBuiltin, "2.0", BOOTSTRAP_REASONS.ADDON_UPGRADE);
    await assertAddonProperties(idBuiltin, "2.0");
    addonBuiltin = await AddonManager.getAddonByID(idBuiltin);
    Assert.equal(
      addonBuiltin.locationName,
      "app-system-addons",
      `got addon ${idBuiltin} in the system-signed updates location`
    );

    await checkAddon(idHotfix, "1.0.1", BOOTSTRAP_REASONS.ADDON_INSTALL);
    await assertAddonProperties(idHotfix, "1.0.1");
    addonHotfix = await AddonManager.getAddonByID(idHotfix);
    Assert.equal(
      addonHotfix.locationName,
      "app-system-addons",
      `got addon ${idHotfix} in the system-signed updates location`
    );

    await promiseShutdownManager();

    info(
      "Mock app upgrade: new app version 2, builtin v2.1, existing system-signed v2"
    );
    AddonTestUtils.updateAppInfo({ version: "2", lastAppVersion: "1" });

    info(
      "Test app version upgrade again with builtin version more recent than system-signed update (should reset system-signed updates)"
    );
    await setupSystemBuiltin(idBuiltin, "2.1", "builtin-ext-path-v2");
    await Promise.all([
      promiseStartupManager(),
      promiseWebExtensionStartup(idBuiltin),
    ]);
    await assertAddonProperties(idBuiltin, "2.1");
    addonBuiltin = await AddonManager.getAddonByID(idBuiltin);
    Assert.equal(
      addonBuiltin.locationName,
      "app-builtin-addons",
      `got addon ${idBuiltin} in the builtin location`
    );

    addonHotfix = await AddonManager.getAddonByID(idHotfix);
    Assert.equal(
      addonHotfix,
      null,
      `addon ${idHotfix} is not found as expected`
    );

    verifySystemAddonSetPref({});

    await promiseShutdownManager();
  }
);

async function testBrokenXPIStates({
  description,
  setupTestCase,
  expectSystemUpdateVersion,
}) {
  info(`Enter test case (${description})`);
  const builtins = [0, 1, 2].map(i => ({
    addon_id: `system${i}@tests.mozilla.org`,
    addon_version: "1.1",
    res_url: `resource://builtin-addon${i}/`,
  }));
  await Promise.all(
    builtins.map(({ addon_id, addon_version }, i) =>
      setupBuiltinExtension(
        {
          manifest: {
            name: `Built-In System Add-on ${i}`,
            version: addon_version,
            browser_specific_settings: {
              gecko: { id: addon_id },
            },
          },
        },
        `builtin-addon${i}`
      )
    )
  );
  AddonTestUtils.updateAppInfo(appInfoInitial);
  await overrideBuiltIns({ builtins });
  const createBuiltinAddonsStartedPromises = () =>
    builtins.map(({ addon_id }) => {
      const msg = `Await ${addon_id} startup`;
      const promise = promiseWebExtensionStartup(addon_id);
      return async () => {
        info(msg);
        await promise;
      };
    });
  let builtinAddonsStartedPromises = createBuiltinAddonsStartedPromises();

  info(`Initialize test case profile`);
  await promiseStartupManager();
  for (const waitForBuiltinStartup of builtinAddonsStartedPromises) {
    await waitForBuiltinStartup();
  }

  const updatedBuiltinAddonId = builtins[2].addon_id;
  async function verifyUpdatedBuiltinAddon() {
    let updatedBuiltinAddon = await AddonManager.getAddonByID(
      updatedBuiltinAddonId
    );
    Assert.equal(
      updatedBuiltinAddon.version,
      "2.0",
      `Got the expected version for the updated builtin addon ${updatedBuiltinAddonId}`
    );
    Assert.equal(
      updatedBuiltinAddon.locationName,
      "app-system-addons",
      `Got the expected locationName for the updated builtin addon ${updatedBuiltinAddonId}`
    );
  }

  info(`Install system-signed update for ${updatedBuiltinAddonId}`);
  const oldUsePrivilegedSignature = (AddonTestUtils.usePrivilegedSignatures =
    () => "system");
  let xpi = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      version: "2.0",
      browser_specific_settings: {
        gecko: { id: updatedBuiltinAddonId },
      },
    },
  });
  AddonTestUtils.usePrivilegedSignatures = oldUsePrivilegedSignature;
  let xml = buildSystemAddonUpdates([
    {
      id: updatedBuiltinAddonId,
      version: "2.0",
      path: xpi.leafName,
      xpi,
    },
  ]);
  await installSystemAddons(xml, [updatedBuiltinAddonId]);
  await verifyUpdatedBuiltinAddon();
  await promiseShutdownManager();
  ok(
    AddonTestUtils.addonStartup.exists(),
    "Expect addonStartup.json.lz4 file to exist"
  );

  info(`Setup test case (${description})`);
  // Run callback to mimic the specific test scenario).
  await setupTestCase();
  info(`Startup addon manager again (${description})`);

  builtinAddonsStartedPromises = createBuiltinAddonsStartedPromises();

  await overrideBuiltIns({ builtins });
  await promiseStartupManager();
  for (const waitForBuiltinStartup of builtinAddonsStartedPromises) {
    await waitForBuiltinStartup();
  }
  // Verify the updated system addon is the one enabled.
  if (expectSystemUpdateVersion) {
    await verifyUpdatedBuiltinAddon();
  }
  // Clean updated system addon.
  await installSystemAddons(buildSystemAddonUpdates([]), []);
  await promiseShutdownManager();
  info(`Exit test case (${description})`);
}

function readAddonStartupData() {
  return Cc["@mozilla.org/addons/addon-manager-startup;1"]
    .getService(Ci.amIAddonManagerStartup)
    .readStartupData();
}

async function promiseWriteAddonStartupData(data) {
  const { JSONFile } = ChromeUtils.importESModule(
    "resource://gre/modules/JSONFile.sys.mjs"
  );
  let jsonFile = new JSONFile({
    path: PathUtils.join(AddonTestUtils.addonStartup.path),
    compression: "lz4",
  });
  jsonFile.data = data;
  await jsonFile._save();
}

// This tests case verifies that in case of a missing or completely corrupted
// xpi states, the builin addons are still installed and started up as expected.
add_task(
  {
    pref_set: [
      ["extensions.skipInstallDefaultThemeForTests", true],
      // Set the same startupScanScopes value set by default on a Firefox Desktop
      // instance.
      ["extensions.startupScanScopes", 0],
    ],
  },
  async function test_missing_xpistate() {
    await testBrokenXPIStates({
      description: "missing addonStartup.json.lz4",
      async setupTestCase() {
        await IOUtils.remove(AddonTestUtils.addonStartup.path);
        ok(
          !AddonTestUtils.addonStartup.exists(),
          "Expect addonStartup.json.lz4 file to be removed"
        );
      },
      expectSystemUpdateVersion: true,
    });
  }
);

// This tests case verifies that in case of a stale addonStartup.json.lz4 state,
// missing builtin addons are still detected early during application startup and
// installed and started as expected.
add_task(
  {
    pref_set: [
      ["extensions.skipInstallDefaultThemeForTests", true],
      // Set the same startupScanScopes value set by default on a Firefox Desktop
      // instance.
      ["extensions.startupScanScopes", 0],
    ],
  },
  async function test_stale_xpistate_app_builtin_addons_location() {
    const builtinId = "system2@tests.mozilla.org";

    function verifyXPIStateData(xpiStateData) {
      ok(
        xpiStateData["app-builtin-addons"],
        "Got app-builtin-addons location in the XPIStates data"
      );
      ok(
        xpiStateData["app-builtin-addons"]?.addons?.[builtinId],
        `Got ${builtinId} app-builtin-addons entry in XPIStates data`
      );
    }

    await testBrokenXPIStates({
      description:
        "stale addonStartup.json.lz4 missing entire app-builtin-addons location",
      async setupTestCase() {
        const xpiStateData = readAddonStartupData();
        verifyXPIStateData(xpiStateData);
        delete xpiStateData["app-builtin-addons"];
        await promiseWriteAddonStartupData(xpiStateData);
      },
      expectSystemUpdateVersion: true,
    });

    await testBrokenXPIStates({
      description:
        "stale addonStartup.json.lz4 missing one of the builtin addons",
      async setupTestCase() {
        const xpiStateData = readAddonStartupData();
        verifyXPIStateData(xpiStateData);
        delete xpiStateData["app-builtin-addons"].addons[builtinId];
        await promiseWriteAddonStartupData(xpiStateData);
      },
      expectSystemUpdateVersion: true,
    });
  }
);

// This tests case verifies that in case of a stale addonStartup.json.lz4 state,
// missing system-signed addons are still detected early during application startup
// and installed and started as expected.
add_task(
  {
    pref_set: [
      ["extensions.skipInstallDefaultThemeForTests", true],
      // Set the same startupScanScopes value set by default on a Firefox Desktop
      // instance.
      ["extensions.startupScanScopes", 0],
    ],
  },
  async function test_stale_xpistate_app_system_addons_location() {
    const builtinId = "system2@tests.mozilla.org";

    function verifyXPIStateData(xpiStateData) {
      ok(
        xpiStateData["app-system-addons"],
        "Got app-system-addons location in the XPIStates data"
      );
      ok(
        xpiStateData["app-system-addons"]?.addons?.[builtinId],
        `Got ${builtinId} app-system-addons entry in XPIStates data`
      );
    }

    await testBrokenXPIStates({
      description:
        "stale addonStartup.json.lz4 missing entire app-system-addons location",
      async setupTestCase() {
        const xpiStateData = readAddonStartupData();
        verifyXPIStateData(xpiStateData);
        delete xpiStateData["app-system-addons"];
        await promiseWriteAddonStartupData(xpiStateData);
      },
      expectSystemUpdateVersion: true,
    });

    await testBrokenXPIStates({
      description:
        "stale addonStartup.json.lz4 missing one of the system-signed addons",
      async setupTestCase() {
        const xpiStateData = readAddonStartupData();
        verifyXPIStateData(xpiStateData);
        delete xpiStateData["app-system-addons"].addons[builtinId];
        await promiseWriteAddonStartupData(xpiStateData);
      },
      expectSystemUpdateVersion: true,
    });
  }
);
