/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// NOTE: this head support file should be used along with the
// one we are importing eslint globals from, and to be listed
// in the xpcshell.toml `head` property value right after it for
// the xpcshell test file that are meant to test Nimbus-driven
// train-hopping.

/* import-globals-from ../../../../extensions/newtab/test/xpcshell/head.js */

/* exported assertNewTabResourceMapping, assertTrainhopAddonNimbusExposure,
 *          assertTrainhopAddonVersionPref,
 *          asyncAssertNewTabAddon, asyncAssertNimbusTrainhopAddonStaged,
 *          cancelPendingInstall, mockAboutNewTabUninit, server,
 *          BUILTIN_ADDON_ID, BUILTIN_ADDON_VERSION,
 *          BUILTIN_LOCATION_NAME, PROFILE_LOCATION_NAME,
 *          TRAINHOP_NIMBUS_FEATURE_ID, TRAINHOP_SCHEDULED_UPDATE_STATE_PREF */

const {
  AboutNewTabResourceMapping,
  BUILTIN_ADDON_ID,
  TRAINHOP_NIMBUS_FEATURE_ID,
  TRAINHOP_XPI_BASE_URL_PREF,
  TRAINHOP_SCHEDULED_UPDATE_STATE_PREF,
} = ChromeUtils.importESModule(
  "resource:///modules/AboutNewTabResourceMapping.sys.mjs"
);
const { ExperimentAPI, NimbusFeatures } = ChromeUtils.importESModule(
  "resource://nimbus/ExperimentAPI.sys.mjs"
);
const { NimbusTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);

const BUILTIN_LOCATION_NAME = "app-builtin-addons";
const PROFILE_LOCATION_NAME = "app-profile";

// Set from the setup task based on the currently installed
// newtab built-in add-on version.
let BUILTIN_ADDON_VERSION;

NimbusTestUtils.init(this);

const server = AddonTestUtils.createHttpServer({ hosts: ["example.com"] });
Services.prefs.setStringPref(TRAINHOP_XPI_BASE_URL_PREF, "http://example.com/");

// Defaults to "system" signedState for xpi with the newtab builtin add-id.
AddonTestUtils.usePrivilegedSignatures = addonId =>
  addonId === BUILTIN_ADDON_ID ? "system" : false;

add_setup(async function nimbusTestsSetup() {
  const { cleanup: nimbusTestCleanup } = await NimbusTestUtils.setupTest();
  registerCleanupFunction(nimbusTestCleanup);

  const builtinAddon = await asyncAssertNewTabAddon({
    locationName: "app-builtin-addons",
  });
  Assert.ok(builtinAddon?.version, "Got a builtin add-on version");
  BUILTIN_ADDON_VERSION = builtinAddon.version;

  // When we stage installs and then cancel them, `XPIInstall` won't be able to
  // remove the staging directory (which is expected to be empty) until the
  // next restart. This causes an `AddonTestUtils` assertion to fail because we
  // don't expect any staging directory at the end of the tests. That's why we
  // remove this directory in the cleanup function defined below.
  //
  // We only remove the staging directory and that will only works if the
  // directory is empty, otherwise an unchaught error will be thrown (on
  // purpose).
  registerCleanupFunction(() => {
    const profileDir = do_get_profile();
    const stagingDir = profileDir.clone();
    stagingDir.append("extensions");
    stagingDir.append("staged");
    stagingDir.exists() && stagingDir.remove(/* recursive */ false);
  });
});

/**
 * Setting up a simulated train-hop add-on version for testing.
 *
 * @param {object} params
 * @param {string} [params.updateAddonVersion]
 *   The version of the train-hop add-on to set in the simulated feature enrolling.
 * @returns {Promise<object>} A promise that resolves with an object containing:
 *   - fakeNimbusVariables: The fake Nimbus feature variables used in the simulated Nimbus feature enrolling.
 *   - nimbusFeatureCleanup: The cleanup function for the Nimbus feature.
 */
async function setupNimbusTrainhopAddon({ updateAddonVersion }) {
  info(`Setting up simulated train-hop add-on version ${updateAddonVersion}`);
  let fakeNewTabXPI = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      version: updateAddonVersion,
      browser_specific_settings: {
        gecko: { id: BUILTIN_ADDON_ID },
      },
    },
    files: {
      "lib/NewTabGleanUtils.sys.mjs": `
        export const NewTabGleanUtils = {
          registerMetricsAndPings() {},
        };
      `,
    },
  });

  const xpi_download_path = "data/newtab.xpi";

  server.registerFile(`/${xpi_download_path}`, fakeNewTabXPI, () => {
    info(`Server got request for ${xpi_download_path}`);
  });

  const fakeNimbusVariables = {
    xpi_download_path,
    addon_version: updateAddonVersion,
  };

  await ExperimentAPI.ready();
  const nimbusFeatureCleanup = await NimbusTestUtils.enrollWithFeatureConfig(
    {
      featureId: TRAINHOP_NIMBUS_FEATURE_ID,
      value: fakeNimbusVariables,
    },
    { isRollout: true }
  );
  // Sanity checks.
  Assert.deepEqual(
    NimbusFeatures[TRAINHOP_NIMBUS_FEATURE_ID].getAllVariables(),
    fakeNimbusVariables,
    "Got the expected variables from the nimbus feature"
  );

  return { fakeNimbusVariables, nimbusFeatureCleanup };
}

/**
 * Verifies that a newtab add-on exists in the expected location and optionally asserts
 * that the add-on has the expected version.
 *
 * @param {object} params
 * @param {string} params.locationName
 *   XPIProvider location name where the newtab add-on is expected to be installed into.
 * @param {string} [params.version]
 *   The expected newtab add-on version (only verified if set).
 * @returns {Promise<AddonWrapper>}
 *   A promise that resolves to the AddonWrapper instance for the newtab add-on.
 */
async function asyncAssertNewTabAddon({ locationName, version }) {
  const newtabAddon = await AddonManager.getAddonByID(BUILTIN_ADDON_ID);
  Assert.equal(
    newtabAddon?.locationName,
    locationName,
    "Got the expected newtab add-on locationName"
  );
  if (version) {
    Assert.equal(
      newtabAddon?.version,
      version,
      "Got the expected newtab add-on version"
    );
  }
  return newtabAddon;
}

/**
 * Verifies that the expected staged installation for the train-hop add-on exists.
 *
 * @param {object} params
 * @param {string} params.updateAddonVersion
 *   The add-on version expected to be installed by the simulated feature enrolling.
 * @returns {Promise<AddonInstall>}
 *   A promise that resolves with to the AddonInstall instance for the train-hop
 *   add-on version expected to be found staged.
 */
async function asyncAssertNimbusTrainhopAddonStaged({ updateAddonVersion }) {
  const pendingInstall = (await AddonManager.getAllInstalls()).find(
    install => install.addon.id === BUILTIN_ADDON_ID
  );
  Assert.equal(
    pendingInstall?.state,
    AddonManager.STATE_POSTPONED,
    "Expect a pending install for the newtab add-on to be found"
  );

  Assert.deepEqual(
    {
      existingVersion: pendingInstall.existingAddon.version,
      existingLocationName: pendingInstall.existingAddon.locationName,
      updateVersion: pendingInstall.addon.version,
      updateLocationName: pendingInstall.addon.locationName,
    },
    {
      existingVersion: BUILTIN_ADDON_VERSION,
      existingLocationName: BUILTIN_LOCATION_NAME,
      updateVersion: updateAddonVersion,
      updateLocationName: PROFILE_LOCATION_NAME,
    },
    "Got the expected version and locationName pendingInstall existing and updated add-on"
  );
  return { pendingInstall };
}

/**
 * Asserts that an exposure event for train-hop Nimbus feature has been recorded or
 * not recorded yet.
 *
 * @param {object}  params
 * @param {boolean} params.expectedExposure
 */
function assertTrainhopAddonNimbusExposure({ expectedExposure }) {
  const enrollmentMetadata =
    NimbusFeatures[TRAINHOP_NIMBUS_FEATURE_ID].getEnrollmentMetadata();
  Assert.deepEqual(
    Glean.nimbusEvents.exposure
      .testGetValue("events")
      ?.map(ev => ev.extra)
      .filter(ev => ev.feature_id == TRAINHOP_NIMBUS_FEATURE_ID) ?? [],
    expectedExposure
      ? [
          {
            feature_id: TRAINHOP_NIMBUS_FEATURE_ID,
            branch: enrollmentMetadata.branch,
            experiment: enrollmentMetadata.slug,
          },
        ]
      : [],
    expectedExposure
      ? "Got the expected exposure Glean event for the newtabTrainhopAddon Nimbus feature"
      : "Got no exposure Glean event for the newtabTrainhopAddon as expected"
  );
}

function assertTrainhopAddonVersionPref(expectedTrainhopAddonVersion) {
  Assert.equal(
    Services.prefs.getStringPref(
      "browser.newtabpage.trainhopAddon.version",
      ""
    ),
    expectedTrainhopAddonVersion,
    expectedTrainhopAddonVersion
      ? "Expect browser.newtab.trainhopAddon.version about:config pref to be set while client is enrolled"
      : "Expect browser.newtab.trainhopAddon.version about:config pref to be empty while client is unenrolled"
  );
}

/**
 * Cancels a pending add-on installation and awaits the cancellation to be completed.
 *
 * @param {AddonInstall} pendingInstall
 *   Instance of the pending AddonInstall to cancel.
 * @returns {Promise<void>}
 *   A promise that resolves when the AddonInstall instance has been successfully cancelled.
 */
async function cancelPendingInstall(pendingInstall) {
  const cancelDeferred = Promise.withResolvers();
  pendingInstall.addListener({
    onInstallCancelled() {
      cancelDeferred.resolve();
    },
  });
  pendingInstall.cancel();
  await cancelDeferred.promise;
}

// Mock browser restart scenario.
function mockAboutNewTabUninit() {
  AboutNewTab.uninit();
  AboutNewTabResourceMapping.initialized = false;
  AboutNewTabResourceMapping._rootURISpec = null;
  AboutNewTabResourceMapping._addonVersion = null;
  AboutNewTabResourceMapping._addonListener = null;
}
