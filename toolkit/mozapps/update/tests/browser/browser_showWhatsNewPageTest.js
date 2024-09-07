/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test if the correct What's New Page will be displayed
// when a major Firefox update is installed by a different profile.
// Also ensures no What's New Page will be displayed when an installed update
// has a new platformVersion.

const UPDATE_PROVIDED_PAGE = "https://default.example.com/";
const UPDATE_PROVIDED_PAGE2 = "https://default2.example.com/";
const NO_POST_UPDATE_PAGE = "about:blank";

const PREF_MSTONE = "browser.startup.homepage_override.mstone";
const PREF_PREV_BUILDID = "browser.startup.homepage_override.buildID";

const DEFAULT_PLATFORM_VERSION = "2.0";
const DEFAULT_OLD_BUILD_ID = "20080811053724";
const DEFAULT_NEW_BUILD_ID = "20080811053725";

const gOrigAppInfo = Services.appinfo;

add_setup(async () => {
  const origMstone = Services.prefs.getCharPref(PREF_MSTONE);
  const origBuildId = Services.prefs.getCharPref(PREF_PREV_BUILDID);
  registerCleanupFunction(() => {
    Services.appinfo = gOrigAppInfo;
    Services.prefs.setCharPref(PREF_MSTONE, origMstone);
    Services.prefs.setCharPref(PREF_PREV_BUILDID, origBuildId);
  });
});

/**
 * Loads an update into the update system and checks that the What's New Page
 * is shown correctly.
 *
 * @param origPlatformVersion
 * @param origBuildId
 *    Version information that should be written into prefs as the last version
 *    that this profile ran
 * @param updatePlatformVersion
 * @param updateAppVersion
 * @param updateBuildId
 * @param updateWnp
 *    Information about an update to load into the update system via XML. If
 *    this were real instead of a test, this information would have come from
 *    Balrog.
 * @param setUpdateHistoryOnly
 *    Normally, this function loads the specified update information such that
 *    it appears that this update has just been installed. If this is set to
 *    `true`, the update will instead be loaded into the update history.
 * @param installedPlatformVersion
 * @param installedAppVersion
 * @param installedBuildId
 *    Information about the version that Firefox is running at after the
 *    (simulated) update.
 *    These default to the corresponding `update*` values if they aren't
 *    specified.
 * @param expectedPostUpdatePage
 *    If provided, this will assert that the post update page shown after the
 *    update matches the one provided.
 * @param expectUpdatePing
 *    If `true`, this will assert that an update success ping is sent with the
 *    correct version information.
 */
async function WnpTest({
  origPlatformVersion,
  origBuildId = DEFAULT_OLD_BUILD_ID,
  updatePlatformVersion,
  updateAppVersion,
  updateBuildId,
  updateWnp,
  setUpdateHistoryOnly = false,
  installedPlatformVersion,
  installedAppVersion,
  installedBuildId,
  expectedPostUpdatePage,
  expectUpdatePing,
}) {
  const archiveChecker = new TelemetryArchiveTesting.Checker();
  await archiveChecker.promiseInit();

  if (origPlatformVersion) {
    logTestInfo(`Setting original platformVersion to ${origPlatformVersion}`);
    Services.prefs.setCharPref(PREF_MSTONE, origPlatformVersion);
  } else {
    origPlatformVersion = Services.prefs.getCharPref(PREF_MSTONE);
    logTestInfo(`Loaded original platformVersion as ${origPlatformVersion}`);
  }
  if (origBuildId) {
    logTestInfo(`Setting original buildID to ${origBuildId}`);
    Services.prefs.setCharPref(PREF_PREV_BUILDID, origBuildId);
  } else {
    origBuildId = Services.prefs.getCharPref(PREF_PREV_BUILDID);
    logTestInfo(`Loaded original buildID as ${origBuildId}`);
  }

  let activeUpdateXML = getLocalUpdatesXMLString("");
  let updateHistoryXML = getLocalUpdatesXMLString("");
  if (updatePlatformVersion || updateAppVersion || updateBuildId) {
    updatePlatformVersion = updatePlatformVersion ?? DEFAULT_PLATFORM_VERSION;
    updateAppVersion = updateAppVersion ?? updatePlatformVersion;
    updateBuildId = updateBuildId ?? DEFAULT_NEW_BUILD_ID;
    updateWnp = updateWnp ?? UPDATE_PROVIDED_PAGE;

    logTestInfo(
      `Faking update with platformVersion=${updatePlatformVersion}, ` +
        `appVersion=${updateAppVersion}, buildID=${updateBuildId}, ` +
        `WNP=${updateWnp}`
    );

    const XML_UPDATE = `<?xml version="1.0"?>
    <updates xmlns="http://www.mozilla.org/2005/app-update">
      <update appVersion="${updateAppVersion}" buildID="${updateBuildId}" channel="nightly"
              displayVersion="Version ${updateAppVersion}" installDate="1238441400314"
              platformVersion="${updatePlatformVersion}" isCompleteUpdate="true" name="Update Test ${updateAppVersion}" type="minor"
              detailsURL="http://example.com/" previousAppVersion="1.0"
              serviceURL="https://example.com/" statusText="The Update was successfully installed"
              foregroundDownload="true"
              actions="showURL"
              openURL="${updateWnp}">
        <patch type="complete" URL="http://example.com/" size="775" selected="true" state="succeeded"/>
      </update>
    </updates>`;

    if (setUpdateHistoryOnly) {
      logTestInfo("Writing update into the update history");
      updateHistoryXML = XML_UPDATE;
    } else {
      logTestInfo("Writing update into the active update XML");
      activeUpdateXML = XML_UPDATE;
    }
  } else {
    logTestInfo("Not faking an update. Both update XMLs will be empty");
  }
  writeUpdatesToXMLFile(activeUpdateXML, true);
  writeUpdatesToXMLFile(updateHistoryXML, false);
  writeStatusFile(STATE_SUCCEEDED);

  // Wait until here to apply the default values for these, since we want the
  // default values to match the values in the update, even if those changed
  installedPlatformVersion = installedPlatformVersion ?? updatePlatformVersion;
  installedAppVersion = installedAppVersion ?? updateAppVersion;
  installedBuildId = installedBuildId ?? updateBuildId;

  const appInfoProps = {};
  if (installedPlatformVersion) {
    appInfoProps.platformVersion = {
      configurable: true,
      enumerable: true,
      writable: false,
      value: installedPlatformVersion,
    };
  }
  if (installedAppVersion) {
    appInfoProps.version = {
      configurable: true,
      enumerable: true,
      writable: false,
      value: installedAppVersion,
    };
  }
  if (installedBuildId) {
    appInfoProps.platformBuildID = {
      configurable: true,
      enumerable: true,
      writable: false,
      value: installedBuildId,
    };
  }
  Services.appinfo = Object.create(gOrigAppInfo, appInfoProps);
  logTestInfo(
    `Set appinfo to use platformVersion=${Services.appinfo.platformVersion}, ` +
      `version=${Services.appinfo.version}, ` +
      `platformBuildID=${Services.appinfo.platformBuildID}`
  );

  await reloadUpdateManagerData(false);

  if (expectedPostUpdatePage) {
    const postUpdatePage = Cc["@mozilla.org/browser/clh;1"]
      .getService(Ci.nsIBrowserHandler)
      .getFirstWindowArgs();
    is(
      postUpdatePage,
      expectedPostUpdatePage,
      "Post Update Page should be correct"
    );
  } else {
    logTestInfo(`Not checking Post Update Page in this test`);
  }

  if (expectUpdatePing) {
    const updatePing = await waitForUpdatePing(archiveChecker, [
      [["payload", "reason"], "success"],
    ]);
    is(
      updatePing.payload.previousVersion,
      origPlatformVersion,
      "Update Ping should have correct previousVersion"
    );
    is(
      updatePing.payload.previousBuildId,
      origBuildId,
      "Update Ping should have correct previousBuildId"
    );
  } else {
    // Ideally we would test that the update ping isn't sent in this case,
    // but we have to wait for a timeout to ensure that an update ping didn't
    // send, which isn't very appealing.
    logTestInfo(`Not checking the update ping in this test`);
  }
}

add_task(async function test_WhatsNewPage() {
  logTestInfo("Initial test");
  await WnpTest({
    origPlatformVersion: "1.0",
    updatePlatformVersion: "2.0",
    updateWnp: UPDATE_PROVIDED_PAGE,
    expectedPostUpdatePage: UPDATE_PROVIDED_PAGE,
    expectUpdatePing: true,
  });

  // Write another update with the same platformVersion.
  logTestInfo("Second update, same platformVersion");
  await WnpTest({
    updatePlatformVersion: "2.0",
    updateAppVersion: "2.1",
    expectedPostUpdatePage: NO_POST_UPDATE_PAGE,
    expectUpdatePing: true,
  });

  // Make sure that if the platform version string in the installed browser
  // doesn't match the one in the XML for that update, we trust the one provided
  // by the browser.
  logTestInfo("Trust built in platformVersion over Balrog, test 1");
  await WnpTest({
    origPlatformVersion: "2.0",
    updatePlatformVersion: "2.0",
    updateAppVersion: "2.1",
    updateWnp: UPDATE_PROVIDED_PAGE2,
    installedPlatformVersion: "3.0",
    expectedPostUpdatePage: UPDATE_PROVIDED_PAGE2,
    expectUpdatePing: true,
  });
  logTestInfo("Trust built in platformVersion over Balrog, test 2");
  await WnpTest({
    origPlatformVersion: "2.0",
    updatePlatformVersion: "3.0",
    updateWnp: UPDATE_PROVIDED_PAGE2,
    installedPlatformVersion: "2.0",
    installedBuildId: DEFAULT_OLD_BUILD_ID,
    expectedPostUpdatePage: NO_POST_UPDATE_PAGE,
    expectUpdatePing: false,
  });

  // Simulate loading a different profile that did not load during the previous updates.
  logTestInfo("Test that a different profile also gets the WNP");
  await WnpTest({
    origPlatformVersion: "2.0",
    updatePlatformVersion: "3.0",
    updateWnp: UPDATE_PROVIDED_PAGE,
    setUpdateHistoryOnly: true,
    expectedPostUpdatePage: UPDATE_PROVIDED_PAGE,
    expectUpdatePing: false,
  });

  // Simulate an update where the appVersion is newer than the appVersion of the running Firefox instance
  // This ensures if the user downgrades, we don't show an inappropriate Whats New Page.
  logTestInfo("Test that a downgraded browser won't show a stale WNP");
  await WnpTest({
    origPlatformVersion: "1.0",
    updatePlatformVersion: "3.0",
    updateAppVersion: "3.0",
    updateWnp: UPDATE_PROVIDED_PAGE,
    installedPlatformVersion: "2.0",
    installedAppVersion: "2.0",
    expectedPostUpdatePage: NO_POST_UPDATE_PAGE,
    expectUpdatePing: true,
  });
});
