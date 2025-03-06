/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

/**
 * This file tests AboutNewTab  for its default URL values, as well as its
 * behaviour when overriding the default URL values.
 */

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);
const { AboutNewTab } = ChromeUtils.importESModule(
  "resource:///modules/AboutNewTab.sys.mjs"
);

AboutNewTab.init();

const IS_RELEASE_OR_BETA = AppConstants.RELEASE_OR_BETA;

const DOWNLOADS_URL =
  "chrome://browser/content/downloads/contentAreaDownloadsView.xhtml";
const SEPARATE_PRIVILEGED_CONTENT_PROCESS_PREF =
  "browser.tabs.remote.separatePrivilegedContentProcess";
const ACTIVITY_STREAM_DEBUG_PREF = "browser.newtabpage.activity-stream.debug";
const SIMPLIFIED_WELCOME_ENABLED_PREF = "browser.aboutwelcome.enabled";

function cleanup() {
  Services.prefs.clearUserPref(SEPARATE_PRIVILEGED_CONTENT_PROCESS_PREF);
  Services.prefs.clearUserPref(ACTIVITY_STREAM_DEBUG_PREF);
  Services.prefs.clearUserPref(SIMPLIFIED_WELCOME_ENABLED_PREF);
  AboutNewTab.resetNewTabURL();
}

registerCleanupFunction(cleanup);

function nextChangeNotificationPromise(aNewURL, testMessage) {
  return new Promise(resolve => {
    Services.obs.addObserver(function observer(aSubject, aTopic, aData) {
      Services.obs.removeObserver(observer, aTopic);
      Assert.equal(aData, aNewURL, testMessage);
      resolve();
    }, "newtab-url-changed");
  });
}

function setPrivilegedContentProcessPref(usePrivilegedContentProcess) {
  if (
    usePrivilegedContentProcess === AboutNewTab.privilegedAboutProcessEnabled
  ) {
    return Promise.resolve();
  }

  let notificationPromise = nextChangeNotificationPromise("about:newtab");

  Services.prefs.setBoolPref(
    SEPARATE_PRIVILEGED_CONTENT_PROCESS_PREF,
    usePrivilegedContentProcess
  );
  return notificationPromise;
}

function addTestsWithPrivilegedContentProcessPref(test) {
  add_task(async () => {
    await setPrivilegedContentProcessPref(true);
    await test();
  });
  add_task(async () => {
    await setPrivilegedContentProcessPref(false);
    await test();
  });
}

function setBoolPrefAndWaitForChange(pref, value, testMessage) {
  return new Promise(resolve => {
    Services.obs.addObserver(function observer(aSubject, aTopic, aData) {
      Services.obs.removeObserver(observer, aTopic);
      Assert.equal(aData, AboutNewTab.newTabURL, testMessage);
      resolve();
    }, "newtab-url-changed");

    Services.prefs.setBoolPref(pref, value);
  });
}

add_task(async function test_as_initial_values() {
  Assert.ok(
    AboutNewTab.activityStreamEnabled,
    ".activityStreamEnabled should be set to the correct initial value"
  );
  // This pref isn't defined on release or beta, so we fall back to false
  Assert.equal(
    AboutNewTab.activityStreamDebug,
    Services.prefs.getBoolPref(ACTIVITY_STREAM_DEBUG_PREF, false),
    ".activityStreamDebug should be set to the correct initial value"
  );
});

/**
 * Test the overriding of the default URL
 */
add_task(async function test_override_activity_stream_disabled() {
  let notificationPromise;

  Assert.ok(
    !AboutNewTab.newTabURLOverridden,
    "Newtab URL should not be overridden"
  );

  // override with some remote URL
  let url = "http://example.com/";
  notificationPromise = nextChangeNotificationPromise(url);
  AboutNewTab.newTabURL = url;
  await notificationPromise;
  Assert.ok(AboutNewTab.newTabURLOverridden, "Newtab URL should be overridden");
  Assert.ok(
    !AboutNewTab.activityStreamEnabled,
    "Newtab activity stream should not be enabled"
  );
  Assert.equal(
    AboutNewTab.newTabURL,
    url,
    "Newtab URL should be the custom URL"
  );

  // test reset with activity stream disabled
  notificationPromise = nextChangeNotificationPromise("about:newtab");
  AboutNewTab.resetNewTabURL();
  await notificationPromise;
  Assert.ok(
    !AboutNewTab.newTabURLOverridden,
    "Newtab URL should not be overridden"
  );
  Assert.equal(
    AboutNewTab.newTabURL,
    "about:newtab",
    "Newtab URL should be the default"
  );

  // test override to a chrome URL
  notificationPromise = nextChangeNotificationPromise(DOWNLOADS_URL);
  AboutNewTab.newTabURL = DOWNLOADS_URL;
  await notificationPromise;
  Assert.ok(AboutNewTab.newTabURLOverridden, "Newtab URL should be overridden");
  Assert.equal(
    AboutNewTab.newTabURL,
    DOWNLOADS_URL,
    "Newtab URL should be the custom URL"
  );

  cleanup();
});

addTestsWithPrivilegedContentProcessPref(
  async function test_override_activity_stream_enabled() {
    Assert.ok(
      !AboutNewTab.newTabURLOverridden,
      "Newtab URL should not be overridden"
    );
    Assert.ok(
      AboutNewTab.activityStreamEnabled,
      "Activity Stream should be enabled"
    );

    // change to a chrome URL while activity stream is enabled
    let notificationPromise = nextChangeNotificationPromise(DOWNLOADS_URL);
    AboutNewTab.newTabURL = DOWNLOADS_URL;
    await notificationPromise;
    Assert.equal(
      AboutNewTab.newTabURL,
      DOWNLOADS_URL,
      "Newtab URL set to chrome url"
    );
    Assert.ok(
      AboutNewTab.newTabURLOverridden,
      "Newtab URL should be overridden"
    );
    Assert.ok(
      !AboutNewTab.activityStreamEnabled,
      "Activity Stream should not be enabled"
    );

    cleanup();
  }
);

addTestsWithPrivilegedContentProcessPref(async function test_default_url() {
  // Only debug variants aren't available on release/beta
  if (!IS_RELEASE_OR_BETA) {
    await setBoolPrefAndWaitForChange(
      ACTIVITY_STREAM_DEBUG_PREF,
      true,
      "A notification occurs after changing the debug pref to true"
    );
    Assert.equal(
      AboutNewTab.activityStreamDebug,
      true,
      "the .activityStreamDebug property is set to true"
    );
    await setBoolPrefAndWaitForChange(
      ACTIVITY_STREAM_DEBUG_PREF,
      false,
      "A notification occurs after changing the debug pref to false"
    );
  } else {
    Services.prefs.setBoolPref(ACTIVITY_STREAM_DEBUG_PREF, true);

    Assert.equal(
      AboutNewTab.activityStreamDebug,
      false,
      "the .activityStreamDebug property is remains false"
    );
  }

  cleanup();
});

/**
 * Tests response to updates to prefs
 */
addTestsWithPrivilegedContentProcessPref(async function test_updates() {
  // Simulates a "cold-boot" situation, with some pref already set before testing a series
  // of changes.
  AboutNewTab.resetNewTabURL(); // need to set manually because pref notifs are off
  let notificationPromise;

  // test update fires on override and reset
  let testURL = "https://example.com/";
  notificationPromise = nextChangeNotificationPromise(
    testURL,
    "a notification occurs on override"
  );
  AboutNewTab.newTabURL = testURL;
  await notificationPromise;

  // from overridden to default
  notificationPromise = nextChangeNotificationPromise(
    "about:newtab",
    "a notification occurs on reset"
  );
  AboutNewTab.resetNewTabURL();
  Assert.ok(
    AboutNewTab.activityStreamEnabled,
    "Activity Stream should be enabled"
  );
  await notificationPromise;

  // reset twice, only one notification for default URL
  notificationPromise = nextChangeNotificationPromise(
    "about:newtab",
    "reset occurs"
  );
  AboutNewTab.resetNewTabURL();
  await notificationPromise;

  cleanup();
});
