/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../../../../extensions/newtab/test/xpcshell/head.js */

/* import-globals-from head_nimbus_trainhop.js */

add_task(async function test_download_and_staged_install_trainhop_addon() {
  Services.fog.testResetFOG();

  // Sanity check (verifies built-in add-on resources have been mapped).
  assertNewTabResourceMapping();
  await asyncAssertNewTabAddon({
    locationName: BUILTIN_LOCATION_NAME,
  });
  assertTrainhopAddonNimbusExposure({ expectedExposure: false });

  const updateAddonVersion = `${BUILTIN_ADDON_VERSION}.123`;

  const { nimbusFeatureCleanup } = await setupNimbusTrainhopAddon({
    updateAddonVersion,
  });
  assertTrainhopAddonVersionPref(updateAddonVersion);

  await AboutNewTabResourceMapping.updateTrainhopAddonState();
  const { pendingInstall } = await asyncAssertNimbusTrainhopAddonStaged({
    updateAddonVersion,
  });

  // Verify that we are still using the New Tab resources from the builtin add-on.
  assertNewTabResourceMapping();
  // Verify that no exposure event has been recorded until the New Tab resources
  // for the train-hop add-on version are actually in use.
  assertTrainhopAddonNimbusExposure({ expectedExposure: false });

  await cancelPendingInstall(pendingInstall);
  await nimbusFeatureCleanup();
  assertTrainhopAddonVersionPref("");
});

add_task(async function test_trainhop_addon_download_errors() {
  server.registerPathHandler("/data/invalid-zip.xpi", (_request, response) => {
    response.write("NOT_A_VALID_XPI");
  });

  const brokenManifestXPI = await AddonTestUtils.createTempXPIFile({
    "manifest.json": "not valid JSON",
  });
  server.registerPathHandler(
    "/data/broken-manifest.xpi",
    (request, response) => {
      server._handler._writeFileResponse(request, brokenManifestXPI, response);
    }
  );

  const invalidManifestXPI = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      version: `${BUILTIN_ADDON_VERSION}.123`,
      browser_specific_settings: {
        gecko: { id: BUILTIN_ADDON_ID },
      },
      // Invalid manifest property that is expected to hit a manifest
      // validation error.
      background: { scripts: "it-should-be-an-array.js" },
    },
  });
  server.registerPathHandler(
    "/data/invalid-manifest.xpi",
    (request, response) => {
      server._handler._writeFileResponse(request, invalidManifestXPI, response);
    }
  );

  const invalidSignatureXPI = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      version: `${BUILTIN_ADDON_VERSION}.123`,
      browser_specific_settings: {
        gecko: { id: BUILTIN_ADDON_ID },
      },
    },
  });
  server.registerPathHandler(
    "/data/invalid-signature.xpi",
    (request, response) => {
      server._handler._writeFileResponse(
        request,
        invalidSignatureXPI,
        response
      );
    }
  );

  await ExperimentAPI.ready();
  await testDownloadError("data/non-existing.xpi");
  await testDownloadError("data/invalid-zip.xpi");
  await testDownloadError("data/broken-manifest.xpi");
  await testDownloadError(
    "data/invalid-manifest.xpi",
    `${BUILTIN_ADDON_VERSION}.123`
  );
  const oldUsePrivilegedSignatures = AddonTestUtils.usePrivilegedSignatures;
  AddonTestUtils.usePrivilegedSignatures = false;
  await testDownloadError(
    "data/invalid-signature.xpi",
    `${BUILTIN_ADDON_VERSION}.123`,
    AddonManager.STATE_CANCELLED
  );
  AddonTestUtils.usePrivilegedSignatures = oldUsePrivilegedSignatures;

  async function testDownloadError(
    xpi_download_path,
    addon_version = "9999.0",
    expectedInstallState = AddonManager.STATE_DOWNLOAD_FAILED
  ) {
    Services.fog.testResetFOG();
    const nimbusFeatureCleanup = await NimbusTestUtils.enrollWithFeatureConfig(
      {
        featureId: TRAINHOP_NIMBUS_FEATURE_ID,
        value: {
          xpi_download_path,
          addon_version,
        },
      },
      { isRollout: true }
    );

    const promiseDownloadFailed =
      AddonTestUtils.promiseInstallEvent("onDownloadFailed");
    const promiseDownloadEnded =
      AddonTestUtils.promiseInstallEvent("onDownloadEnded");

    info("Trigger download and install train-hop add-on version");
    const promiseTrainhopRequest =
      AboutNewTabResourceMapping.updateTrainhopAddonState();

    info("Wait for AddonManager onDownloadFailed");
    const [install] = await Promise.race([
      promiseDownloadFailed,
      // Ensure the test fails right away if the unexpected
      // onDownloadEnded install event is resolved.
      promiseDownloadEnded,
    ]);

    Assert.equal(
      install.state,
      expectedInstallState,
      `Expect install state to be ${AddonManager._states.get(expectedInstallState)}`
    );

    info("Wait for updateTrainhopAddonState call to be resolved as expected");
    await promiseTrainhopRequest;

    Assert.deepEqual(
      await AddonManager.getAllInstalls(),
      [],
      "Expect no pending install to be found"
    );

    assertTrainhopAddonNimbusExposure({ expectedExposure: false });
    await nimbusFeatureCleanup();
  }
});

add_task(async function test_trainhop_cancel_on_version_check() {
  await testTrainhopCancelOnVersionCheck({
    updateAddonVersion: BUILTIN_ADDON_VERSION,
    message:
      "Test train-hop add-on version equal to the built-in add-on version",
  });
  await testTrainhopCancelOnVersionCheck({
    updateAddonVersion: "140.0.1",
    message:
      "Test train-hop add-on version lower than the built-in add-on version",
  });

  async function testTrainhopCancelOnVersionCheck({
    updateAddonVersion,
    message,
  }) {
    Services.fog.testResetFOG();
    info(message);
    // Sanity check (verifies built-in add-on resources have been mapped).
    assertNewTabResourceMapping();
    assertTrainhopAddonNimbusExposure({ expectedExposure: false });

    await asyncAssertNewTabAddon({
      locationName: "app-builtin-addons",
    });
    const { nimbusFeatureCleanup } = await setupNimbusTrainhopAddon({
      updateAddonVersion,
    });

    await AboutNewTabResourceMapping.updateTrainhopAddonState();
    Assert.deepEqual(
      await AddonManager.getAllInstalls(),
      [],
      "Expect no pending install to be found"
    );

    info("Verify the built-in version is still the one installed");
    await asyncAssertNewTabAddon({
      locationName: "app-builtin-addons",
      version: BUILTIN_ADDON_VERSION,
    });
    // Verify that we are still using the New Tab resources from the builtin add-on.
    assertNewTabResourceMapping();
    assertTrainhopAddonNimbusExposure({ expectedExposure: false });

    await nimbusFeatureCleanup();
  }
});

add_task(async function test_trainhop_addon_after_browser_restart() {
  // Sanity check (verifies built-in add-on resources have been mapped).
  assertNewTabResourceMapping();
  await asyncAssertNewTabAddon({
    locationName: BUILTIN_LOCATION_NAME,
  });
  assertTrainhopAddonVersionPref("");

  const updateAddonVersion = `${BUILTIN_ADDON_VERSION}.123`;

  const { nimbusFeatureCleanup } = await setupNimbusTrainhopAddon({
    updateAddonVersion,
  });
  assertTrainhopAddonVersionPref(updateAddonVersion);

  await AboutNewTabResourceMapping.updateTrainhopAddonState();
  await asyncAssertNimbusTrainhopAddonStaged({
    updateAddonVersion,
  });
  // Verify that we are still using the New Tab resources from the builtin add-on.
  assertNewTabResourceMapping();

  info(
    "Simulated browser restart while train-hop add-on is pending installation"
  );
  mockAboutNewTabUninit();
  await AddonTestUtils.promiseRestartManager();
  AboutNewTab.init();

  await asyncAssertNewTabAddon({
    locationName: PROFILE_LOCATION_NAME,
    version: updateAddonVersion,
  });
  const trainhopAddonPolicy = WebExtensionPolicy.getByID(BUILTIN_ADDON_ID);
  Assert.equal(
    trainhopAddonPolicy?.extension?.version,
    updateAddonVersion,
    "Got newtab WebExtensionPolicy instance for the train-hop add-on version"
  );

  assertNewTabResourceMapping(trainhopAddonPolicy.extension.rootURI.spec);

  Assert.deepEqual(
    await AddonManager.getAllInstalls(),
    [],
    "Expect no pending install to be found"
  );

  await AboutNewTabResourceMapping.updateTrainhopAddonState();
  Assert.deepEqual(
    await AddonManager.getAllInstalls(),
    [],
    "Expect no additional pending install for the same train-hop add-on version"
  );

  assertTrainhopAddonNimbusExposure({ expectedExposure: true });
  assertTrainhopAddonVersionPref(updateAddonVersion);

  info("Simulate newtabTrainhopAddon nimbus feature unenrolled");
  await nimbusFeatureCleanup();
  assertTrainhopAddonVersionPref("");

  // Expect train-hop add-on to not be uninstalled yet because it is still
  // used by newtab resources mapping.
  await AboutNewTabResourceMapping.updateTrainhopAddonState();
  assertNewTabResourceMapping(trainhopAddonPolicy.extension.rootURI.spec);
  await asyncAssertNewTabAddon({
    locationName: PROFILE_LOCATION_NAME,
    version: updateAddonVersion,
  });

  info(
    "Simulated browser restart while newtabTrainhopAddon nimbus feature is unenrolled"
  );
  mockAboutNewTabUninit();
  await AddonTestUtils.promiseRestartManager();
  AboutNewTab.init();

  // Expected bundled newtab resources mapping for this session.
  assertNewTabResourceMapping();
  await AboutNewTabResourceMapping.updateTrainhopAddonState();
  await asyncAssertNewTabAddon({
    locationName: BUILTIN_LOCATION_NAME,
    version: BUILTIN_ADDON_VERSION,
  });
});

add_task(async function test_builtin_version_upgrades() {
  // Sanity check (verifies built-in addon resources have been mapped).
  assertNewTabResourceMapping();
  await asyncAssertNewTabAddon({
    locationName: BUILTIN_LOCATION_NAME,
    version: BUILTIN_ADDON_VERSION,
  });
  assertTrainhopAddonVersionPref("");

  const updateAddonVersion = `${BUILTIN_ADDON_VERSION}.123`;

  const { nimbusFeatureCleanup } = await setupNimbusTrainhopAddon({
    updateAddonVersion,
  });
  assertTrainhopAddonVersionPref(updateAddonVersion);

  await AboutNewTabResourceMapping.updateTrainhopAddonState();
  await asyncAssertNimbusTrainhopAddonStaged({
    updateAddonVersion,
  });
  // Verify that we are still using the New Tab resources from the builtin add-on.
  assertNewTabResourceMapping();

  info(
    "Simulated browser restart while train-hop add-on is pending installation"
  );
  mockAboutNewTabUninit();
  await AddonTestUtils.promiseRestartManager();
  AboutNewTab.init();

  await asyncAssertNewTabAddon({
    locationName: PROFILE_LOCATION_NAME,
    version: updateAddonVersion,
  });
  const trainhopAddonPolicy = WebExtensionPolicy.getByID(BUILTIN_ADDON_ID);
  Assert.equal(
    trainhopAddonPolicy?.extension?.version,
    updateAddonVersion,
    "Got newtab WebExtensionPolicy instance for the train-hop add-on version"
  );
  assertNewTabResourceMapping(trainhopAddonPolicy.extension.rootURI.spec);

  info(
    "Simulated browser restart with a builtin add-on version higher than the train-hop add-on version"
  );
  // Mock a builtin version with an add-on version higher than the train-hop add-on version.
  const fakeUpdatedBuiltinVersion = "9999.0";
  const restoreBuiltinAddonsSubstitution =
    await overrideBuiltinAddonsSubstitution(fakeUpdatedBuiltinVersion);

  mockAboutNewTabUninit();
  await AddonTestUtils.promiseRestartManager();
  AboutNewTab.init();
  assertNewTabResourceMapping();
  await AboutNewTabResourceMapping.updateTrainhopAddonState();
  // Expect the newtab xpi to have been uninstalled and the updated
  // builtin add-on to be the newtab add-on version becoming active.
  await asyncAssertNewTabAddon({
    locationName: BUILTIN_LOCATION_NAME,
    version: fakeUpdatedBuiltinVersion,
  });
  Assert.deepEqual(
    await AddonManager.getAllInstalls(),
    [],
    "Expect no pending install to be found"
  );

  // Cleanup
  mockAboutNewTabUninit();
  await restoreBuiltinAddonsSubstitution();
  await AddonTestUtils.promiseRestartManager();
  AboutNewTab.init();
  assertNewTabResourceMapping();
  await asyncAssertNewTabAddon({
    locationName: BUILTIN_LOCATION_NAME,
    version: BUILTIN_ADDON_VERSION,
  });
  await nimbusFeatureCleanup();

  async function overrideBuiltinAddonsSubstitution(updatedBuiltinVersion) {
    const { ExtensionTestCommon } = ChromeUtils.importESModule(
      "resource://testing-common/ExtensionTestCommon.sys.mjs"
    );
    const fakeBuiltinAddonsDir = AddonTestUtils.tempDir.clone();
    fakeBuiltinAddonsDir.append("builtin-addons-override");
    const addonDir = fakeBuiltinAddonsDir.clone();
    addonDir.append("newtab");
    await AddonTestUtils.promiseWriteFilesToDir(
      addonDir.path,
      ExtensionTestCommon.generateFiles({
        manifest: {
          version: updatedBuiltinVersion,
          browser_specific_settings: {
            gecko: { id: BUILTIN_ADDON_ID },
          },
        },
      })
    );
    const resProto = Cc[
      "@mozilla.org/network/protocol;1?name=resource"
    ].getService(Ci.nsIResProtocolHandler);
    let defaultBuiltinAddonsSubstitution =
      resProto.getSubstitution("builtin-addons");
    resProto.setSubstitutionWithFlags(
      "builtin-addons",
      Services.io.newFileURI(fakeBuiltinAddonsDir),
      Ci.nsISubstitutingProtocolHandler.ALLOW_CONTENT_ACCESS
    );

    // Verify we mocked an updated newtab builtin manifest as expected.
    const mockedManifest = await fetch(
      "resource://builtin-addons/newtab/manifest.json"
    ).then(r => r.json());
    Assert.equal(
      mockedManifest.version,
      fakeUpdatedBuiltinVersion,
      "Got the expected manifest version in the mocked builtin add-on manifest"
    );

    // Update built_in_addons.json accordingly.
    await overrideBuiltinsNewTabVersion(updatedBuiltinVersion);

    return async () => {
      await overrideBuiltinsNewTabVersion(BUILTIN_ADDON_VERSION);
      resProto.setSubstitutionWithFlags(
        "builtin-addons",
        defaultBuiltinAddonsSubstitution,
        Ci.nsISubstitutingProtocolHandler.ALLOW_CONTENT_ACCESS
      );
      fakeBuiltinAddonsDir.remove(true);
    };
  }

  async function overrideBuiltinsNewTabVersion(addon_version) {
    // Override newtab builtin version in built_in_addons.json metadata.
    const builtinsConfig = await fetch(
      "chrome://browser/content/built_in_addons.json"
    ).then(res => res.json());
    await AddonTestUtils.overrideBuiltIns({
      system: [],
      builtins: builtinsConfig.builtins
        .filter(entry => entry.addon_id === BUILTIN_ADDON_ID)
        .map(entry => {
          entry.addon_version = addon_version;
          return entry;
        }),
    });
  }
});

add_task(async function test_nonsystem_xpi_uninstalled() {
  // Sanity check (verifies builtin add-on resources have been mapped).
  assertNewTabResourceMapping();

  const updateAddonVersion = `${BUILTIN_ADDON_VERSION}.123`;
  const { nimbusFeatureCleanup } = await setupNimbusTrainhopAddon({
    updateAddonVersion,
  });
  assertTrainhopAddonVersionPref(updateAddonVersion);
  await AboutNewTabResourceMapping.updateTrainhopAddonState();

  info("Simulated restart after train-hop add-on version install pending");
  mockAboutNewTabUninit();
  await AddonTestUtils.promiseRestartManager();
  AboutNewTab.init();

  await asyncAssertNewTabAddon({
    locationName: PROFILE_LOCATION_NAME,
    version: updateAddonVersion,
  });

  // Install non-system signed newtab XPI (Expected to be installed
  // right away because the fake train-hop add-on version doesn't
  // have an onUpdateAvailable listener).
  const xpiVersion = `${BUILTIN_ADDON_VERSION}.456`;
  let extension = await ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      version: xpiVersion,
      browser_specific_settings: {
        gecko: { id: BUILTIN_ADDON_ID },
      },
    },
  });
  const oldUsePrivilegedSignatures = AddonTestUtils.usePrivilegedSignatures;
  AddonTestUtils.usePrivilegedSignatures = false;
  await extension.startup();
  AddonTestUtils.usePrivilegedSignatures = oldUsePrivilegedSignatures;

  let addon = await asyncAssertNewTabAddon({
    locationName: PROFILE_LOCATION_NAME,
    version: xpiVersion,
  });
  Assert.deepEqual(
    addon.signedState,
    AddonManager.SIGNEDSTATE_SIGNED,
    "Got the expected signedState for the installed XPI version"
  );

  mockAboutNewTabUninit();
  await AddonTestUtils.promiseRestartManager();
  AboutNewTab.init();
  assertNewTabResourceMapping();
  await AboutNewTabResourceMapping.updateTrainhopAddonState();
  // Expect the newtab xpi to have been uninstalled and the updated
  // builtin add-on to be the newtab add-on version becoming active.
  await asyncAssertNewTabAddon({
    locationName: BUILTIN_LOCATION_NAME,
    version: BUILTIN_ADDON_VERSION,
  });
  // Along with uninstalling the non-system signed xpi we expect the
  // call to updateTrainhopAddonState to be installing the original
  // train-hop add-on version again.
  const { pendingInstall } = await asyncAssertNimbusTrainhopAddonStaged({
    updateAddonVersion,
  });
  await cancelPendingInstall(pendingInstall);

  await nimbusFeatureCleanup();
});
