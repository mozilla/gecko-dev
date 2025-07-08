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

  await ExperimentAPI.ready();
  await testDownloadError("data/non-existing.xpi");
  await testDownloadError("data/invalid-zip.xpi");
  await testDownloadError("data/broken-manifest.xpi");
  await testDownloadError("data/invalid-manifest.xpi");

  async function testDownloadError(xpi_download_path) {
    Services.fog.testResetFOG();
    const nimbusFeatureCleanup = await NimbusTestUtils.enrollWithFeatureConfig(
      {
        featureId: TRAINHOP_NIMBUS_FEATURE_ID,
        value: {
          xpi_download_path,
          addon_version: "9999.0",
        },
      },
      { isRollout: true }
    );

    const promiseDownloadFailed =
      AddonTestUtils.promiseInstallEvent("onDownloadFailed");

    info("Trigger download and install train-hop add-on version");
    const promiseTrainhopRequest =
      AboutNewTabResourceMapping.updateTrainhopAddonState();

    info("Wait for AddonManager onDownloadFailed");
    const [install] = await promiseDownloadFailed;
    Assert.equal(
      install.state,
      AddonManager.STATE_DOWNLOAD_FAILED,
      "Expect install state to be STATE_DOWNLOAD_FAILED"
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

  // Mock browser restart scenario.
  const aboutNewTabUninit = async () => {
    AboutNewTab.uninit();
    AboutNewTabResourceMapping.initialized = false;
    AboutNewTabResourceMapping._rootURISpec = null;
    AboutNewTabResourceMapping._addonVersion = null;
    AboutNewTabResourceMapping._addonListener = null;
  };

  info(
    "Simulated browser restart while train-hop add-on is pending installation"
  );
  await aboutNewTabUninit();
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
  await aboutNewTabUninit();
  await AddonTestUtils.promiseRestartManager();
  AboutNewTab.init();

  // Expected bundled newtab resources mapping for this session.
  assertNewTabResourceMapping();
  await AboutNewTabResourceMapping.updateTrainhopAddonState();
  await asyncAssertNewTabAddon({
    locationName: BUILTIN_LOCATION_NAME,
    version: BUILTIN_ADDON_VERSION,
  });

  // Test case cleanup.
  await aboutNewTabUninit();
  AboutNewTab.init();
  await nimbusFeatureCleanup();
});
