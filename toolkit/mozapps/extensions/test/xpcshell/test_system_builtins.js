/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Additional tests specifically targeting system addons installed from assets bundled in the omni jar.

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
      // TODO(Bug 1966736): set to true along with the changes needed to make sure
      // the system-signed updated versions to built-in add-ons are the ones
      // activated on regular Firefox startups (startups without application
      // version changes) when the system-addons location data from
      // addonStartup.json.lz4 is lost or stale.
      expectSystemUpdateVersion: false,
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
  async function test_stale_xpistate() {
    async function setupTestCase() {
      const { JSONFile } = ChromeUtils.importESModule(
        "resource://gre/modules/JSONFile.sys.mjs"
      );
      const aomStartup = Cc[
        "@mozilla.org/addons/addon-manager-startup;1"
      ].getService(Ci.amIAddonManagerStartup);
      const xpiStateData = aomStartup.readStartupData();
      ok(
        xpiStateData["app-builtin-addons"],
        "Got app-builtin-addons location in the XPIStates data"
      );
      const builtinId = "system2@tests.mozilla.org";
      ok(
        xpiStateData["app-builtin-addons"]?.addons?.[builtinId],
        `Got ${builtinId} entry in XPIStates data`
      );
      delete xpiStateData["app-builtin-addons"].addons[builtinId];
      let jsonFile = new JSONFile({
        path: PathUtils.join(AddonTestUtils.addonStartup.path),
        compression: "lz4",
      });
      jsonFile.data = xpiStateData;
      await jsonFile._save();
    }
    await testBrokenXPIStates({
      description:
        "stale addonStartup.json.lz4 missing one of the builtin addons",
      setupTestCase,
      expectSystemUpdateVersion: true,
    });
  }
);
