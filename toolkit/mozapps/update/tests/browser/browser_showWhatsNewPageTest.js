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
let origAppInfo = Services.appinfo;

add_setup(() => {
  registerCleanupFunction(() => {
    Services.appinfo = origAppInfo;
  });
});

add_task(async function test_WhatsNewPage() {
  let originalMstone = Services.prefs.getCharPref(PREF_MSTONE);
  // Set the preferences needed for the test: they will be cleared up
  // after it runs.
  await SpecialPowers.pushPrefEnv({ set: [[PREF_MSTONE, originalMstone]] });
  simulateUpdateToVersion("2.0", "2.0", UPDATE_PROVIDED_PAGE);
  setAppPlatformVersion("2.0");

  // Mimic the case where the update is newer than the previous homepage override milestone.
  // The simulated update is version 2.0.
  Services.prefs.setCharPref(PREF_MSTONE, "1.0");
  reloadUpdateManagerData(false);

  is(
    getPostUpdatePage(),
    UPDATE_PROVIDED_PAGE,
    "Post-update page was provided by active-update.xml, should be https://default.example.com/"
  );

  // Write another update with the same platformVersion.
  simulateUpdateToVersion("2.0", "2.0", UPDATE_PROVIDED_PAGE2);
  setAppPlatformVersion("2.0");
  reloadUpdateManagerData(false);

  is(
    getPostUpdatePage(),
    NO_POST_UPDATE_PAGE,
    "Post-update page should be about:blank."
  );

  // Change Services.appinfo.platformVersion to be "3.0", newer than
  // browser.startup.homepage_override.mstone. But keep XML platformVersion the same
  // to ensure correct behaviour if the update server sends bad XML data.
  simulateUpdateToVersion("2.0", "2.1", UPDATE_PROVIDED_PAGE2);
  setAppPlatformVersion("3.0");
  reloadUpdateManagerData(false);

  is(
    getPostUpdatePage(),
    UPDATE_PROVIDED_PAGE2,
    "Post-update page should be https://default2.example.com/."
  );

  // Simulate loading a different profile that did not load during the previous updates.
  Services.prefs.setCharPref(PREF_MSTONE, "1.0");
  reloadUpdateManagerData(false);

  is(
    getPostUpdatePage(),
    UPDATE_PROVIDED_PAGE2,
    "Post-update page should be https://default2.example.com/."
  );

  // Simulate an update where the appVersion is newer than the appVersion of the running Firefox instance
  // This ensures if the user downgrades, we don't show an inappropriate Whats New Page.
  simulateUpdateToVersion("99999999.0", "99999999.0", UPDATE_PROVIDED_PAGE2);
  setAppPlatformVersion("99999999.0");
  reloadUpdateManagerData(false);
  is(
    getPostUpdatePage(),
    NO_POST_UPDATE_PAGE,
    "Post-update page should be about:blank."
  );
});

function getPostUpdatePage() {
  return Cc["@mozilla.org/browser/clh;1"].getService(Ci.nsIBrowserHandler)
    .defaultArgs;
}

function setAppPlatformVersion(version) {
  let originalAppInfo = Services.appinfo;

  let mockAppInfo = Object.create(originalAppInfo, {
    platformVersion: {
      configurable: true,
      enumerable: true,
      writable: false,
      value: version,
    },
  });
  Services.appinfo = mockAppInfo;
}

function simulateUpdateToVersion(platformVersion, appVersion, updateURL) {
  const XML_UPDATE = `<?xml version="1.0"?>
  <updates xmlns="http://www.mozilla.org/2005/app-update">
    <update appVersion="${appVersion}" buildID="20080811053724" channel="nightly"
            displayVersion="Version ${appVersion}" installDate="1238441400314"
            platformVersion="${platformVersion}" isCompleteUpdate="true" name="Update Test ${appVersion}" type="minor"
            detailsURL="http://example.com/" previousAppVersion="1.0"
            serviceURL="https://example.com/" statusText="The Update was successfully installed"
            foregroundDownload="true"
            actions="showURL"
            openURL="${updateURL}">
      <patch type="complete" URL="http://example.com/" size="775" selected="true" state="succeeded"/>
    </update>
  </updates>`;
  writeUpdatesToXMLFile(XML_UPDATE, true);
  writeStatusFile(STATE_SUCCEEDED);
}
