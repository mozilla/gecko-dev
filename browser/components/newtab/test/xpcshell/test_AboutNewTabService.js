/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Preferences.jsm");
ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");

const IS_RELEASE_OR_BETA = AppConstants.RELEASE_OR_BETA;

const DOWNLOADS_URL = "chrome://browser/content/downloads/contentAreaDownloadsView.xul";
const SEPARATE_PRIVILEGED_CONTENT_PROCESS_PREF = "browser.tabs.remote.separatePrivilegedContentProcess";
const ACTIVITY_STREAM_PRERENDER_PREF = "browser.newtabpage.activity-stream.prerender";
const ACTIVITY_STREAM_DEBUG_PREF = "browser.newtabpage.activity-stream.debug";

function cleanup() {
  Services.prefs.clearUserPref(SEPARATE_PRIVILEGED_CONTENT_PROCESS_PREF);
  Services.prefs.clearUserPref(ACTIVITY_STREAM_PRERENDER_PREF);
  Services.prefs.clearUserPref(ACTIVITY_STREAM_DEBUG_PREF);
  aboutNewTabService.resetNewTabURL();
}

registerCleanupFunction(cleanup);

let ACTIVITY_STREAM_PRERENDER_URL;
let ACTIVITY_STREAM_PRERENDER_DEBUG_URL;
let ACTIVITY_STREAM_URL;
let ACTIVITY_STREAM_DEBUG_URL;

function setExpectedUrlsWithScripts() {
  ACTIVITY_STREAM_PRERENDER_URL = "resource://activity-stream/prerendered/en-US/activity-stream-prerendered.html";
  ACTIVITY_STREAM_PRERENDER_DEBUG_URL = "resource://activity-stream/prerendered/static/activity-stream-prerendered-debug.html";
  ACTIVITY_STREAM_URL = "resource://activity-stream/prerendered/en-US/activity-stream.html";
  ACTIVITY_STREAM_DEBUG_URL = "resource://activity-stream/prerendered/static/activity-stream-debug.html";
}

function setExpectedUrlsWithoutScripts() {
  ACTIVITY_STREAM_PRERENDER_URL = "resource://activity-stream/prerendered/en-US/activity-stream-prerendered-noscripts.html";
  ACTIVITY_STREAM_URL = "resource://activity-stream/prerendered/en-US/activity-stream-noscripts.html";

  // Debug urls are the same as non-debug because debug scripts load dynamically
  ACTIVITY_STREAM_PRERENDER_DEBUG_URL = ACTIVITY_STREAM_PRERENDER_URL;
  ACTIVITY_STREAM_DEBUG_URL = ACTIVITY_STREAM_URL;
}

function nextChangeNotificationPromise(aNewURL, testMessage) {
  return new Promise(resolve => {
    Services.obs.addObserver(function observer(aSubject, aTopic, aData) { // jshint unused:false
      Services.obs.removeObserver(observer, aTopic);
      Assert.equal(aData, aNewURL, testMessage);
      resolve();
    }, "newtab-url-changed");
  });
}

function setPrivilegedContentProcessPref(usePrivilegedContentProcess) {
  if (usePrivilegedContentProcess === Services.prefs.getBoolPref(SEPARATE_PRIVILEGED_CONTENT_PROCESS_PREF)) {
    return Promise.resolve();
  }

  let notificationPromise = nextChangeNotificationPromise("about:newtab");
  Services.prefs.setBoolPref(SEPARATE_PRIVILEGED_CONTENT_PROCESS_PREF, usePrivilegedContentProcess);
  return notificationPromise;
}

// Default expected URLs to files with scripts in them.
setExpectedUrlsWithScripts();

function addTestsWithPrivilegedContentProcessPref(test) {
  add_task(async () => {
    await setPrivilegedContentProcessPref(true);
    setExpectedUrlsWithoutScripts();
    await test();
  });
  add_task(async () => {
    await setPrivilegedContentProcessPref(false);
    setExpectedUrlsWithScripts();
    await test();
  });
}

function setBoolPrefAndWaitForChange(pref, value, testMessage) {
  return new Promise(resolve => {
    Services.obs.addObserver(function observer(aSubject, aTopic, aData) { // jshint unused:false
      Services.obs.removeObserver(observer, aTopic);
      Assert.equal(aData, aboutNewTabService.newTabURL, testMessage);
      resolve();
    }, "newtab-url-changed");

    Services.prefs.setBoolPref(pref, value);
  });
}

function setupASPrerendered() {
  if (Services.prefs.getBoolPref(ACTIVITY_STREAM_PRERENDER_PREF)) {
    return Promise.resolve();
  }

  let notificationPromise = nextChangeNotificationPromise("about:newtab");
  Services.prefs.setBoolPref(ACTIVITY_STREAM_PRERENDER_PREF, true);
  return notificationPromise;
}

add_task(async function test_as_and_prerender_initialized() {
  Assert.ok(aboutNewTabService.activityStreamEnabled,
    ".activityStreamEnabled should be set to the correct initial value");
  Assert.equal(aboutNewTabService.activityStreamPrerender, Services.prefs.getBoolPref(ACTIVITY_STREAM_PRERENDER_PREF),
    ".activityStreamPrerender should be set to the correct initial value");
  // This pref isn't defined on release or beta, so we fall back to false
  Assert.equal(aboutNewTabService.activityStreamDebug, Services.prefs.getBoolPref(ACTIVITY_STREAM_DEBUG_PREF, false),
    ".activityStreamDebug should be set to the correct initial value");
});

/**
 * Test the overriding of the default URL
 */
add_task(async function test_override_activity_stream_disabled() {
  let notificationPromise;

  // override with some remote URL
  let url = "http://example.com/";
  notificationPromise = nextChangeNotificationPromise(url);
  aboutNewTabService.newTabURL = url;
  await notificationPromise;
  Assert.ok(aboutNewTabService.overridden, "Newtab URL should be overridden");
  Assert.ok(!aboutNewTabService.activityStreamEnabled, "Newtab activity stream should not be enabled");
  Assert.equal(aboutNewTabService.newTabURL, url, "Newtab URL should be the custom URL");

  // test reset with activity stream disabled
  notificationPromise = nextChangeNotificationPromise("about:newtab");
  aboutNewTabService.resetNewTabURL();
  await notificationPromise;
  Assert.ok(!aboutNewTabService.overridden, "Newtab URL should not be overridden");
  Assert.equal(aboutNewTabService.newTabURL, "about:newtab", "Newtab URL should be the default");

  // test override to a chrome URL
  notificationPromise = nextChangeNotificationPromise(DOWNLOADS_URL);
  aboutNewTabService.newTabURL = DOWNLOADS_URL;
  await notificationPromise;
  Assert.ok(aboutNewTabService.overridden, "Newtab URL should be overridden");
  Assert.equal(aboutNewTabService.newTabURL, DOWNLOADS_URL, "Newtab URL should be the custom URL");

  cleanup();
});

addTestsWithPrivilegedContentProcessPref(async function test_override_activity_stream_enabled() {
  let notificationPromise = await setupASPrerendered();

  Assert.equal(aboutNewTabService.defaultURL, ACTIVITY_STREAM_PRERENDER_URL,
    "Newtab URL should be the default activity stream prerendered URL");
  Assert.ok(!aboutNewTabService.overridden, "Newtab URL should not be overridden");
  Assert.ok(aboutNewTabService.activityStreamEnabled, "Activity Stream should be enabled");
  Assert.ok(aboutNewTabService.activityStreamPrerender, "Activity Stream should be prerendered");

  // change to a chrome URL while activity stream is enabled
  notificationPromise = nextChangeNotificationPromise(DOWNLOADS_URL);
  aboutNewTabService.newTabURL = DOWNLOADS_URL;
  await notificationPromise;
  Assert.equal(aboutNewTabService.newTabURL, DOWNLOADS_URL,
               "Newtab URL set to chrome url");
  Assert.equal(aboutNewTabService.defaultURL, ACTIVITY_STREAM_PRERENDER_URL,
               "Newtab URL defaultURL still set to the default activity stream prerendered URL");
  Assert.ok(aboutNewTabService.overridden, "Newtab URL should be overridden");
  Assert.ok(!aboutNewTabService.activityStreamEnabled, "Activity Stream should not be enabled");

  cleanup();
});

addTestsWithPrivilegedContentProcessPref(async function test_default_url() {
  await setupASPrerendered();

  Assert.equal(aboutNewTabService.defaultURL, ACTIVITY_STREAM_PRERENDER_URL,
    "Newtab defaultURL initially set to prerendered AS url");

  // Only debug variants aren't available on release/beta
  if (!IS_RELEASE_OR_BETA) {
    await setBoolPrefAndWaitForChange(ACTIVITY_STREAM_DEBUG_PREF, true,
      "A notification occurs after changing the debug pref to true");
    Assert.equal(aboutNewTabService.activityStreamDebug, true,
      "the .activityStreamDebug property is set to true");
    Assert.equal(aboutNewTabService.defaultURL, ACTIVITY_STREAM_PRERENDER_DEBUG_URL,
      "Newtab defaultURL set to debug prerendered AS url after the pref has been changed");
    await setBoolPrefAndWaitForChange(ACTIVITY_STREAM_PRERENDER_PREF, false,
      "A notification occurs after changing the prerender pref to false");
    Assert.equal(aboutNewTabService.defaultURL, ACTIVITY_STREAM_DEBUG_URL,
      "Newtab defaultURL set to un-prerendered AS with debug if prerender is false and debug is true");
    await setBoolPrefAndWaitForChange(ACTIVITY_STREAM_DEBUG_PREF, false,
      "A notification occurs after changing the debug pref to false");
  } else {
    Services.prefs.setBoolPref(ACTIVITY_STREAM_DEBUG_PREF, true);

    Assert.equal(aboutNewTabService.activityStreamDebug, false,
      "the .activityStreamDebug property is remains false");
    await setBoolPrefAndWaitForChange(ACTIVITY_STREAM_PRERENDER_PREF, false,
      "A notification occurs after changing the prerender pref to false");
  }

  Assert.equal(aboutNewTabService.defaultURL, ACTIVITY_STREAM_URL,
    "Newtab defaultURL set to un-prerendered AS if prerender is false and debug is false");

  cleanup();
});

addTestsWithPrivilegedContentProcessPref(async function test_welcome_url() {
  await setupASPrerendered();

  Assert.equal(aboutNewTabService.activityStreamPrerender, true,
    "Prerendering is enabled by default.");
  Assert.equal(aboutNewTabService.welcomeURL, ACTIVITY_STREAM_URL,
    "Newtab welcomeURL set to un-prerendered AS when prerendering enabled and debug disabled.");
  await setBoolPrefAndWaitForChange(ACTIVITY_STREAM_PRERENDER_PREF, false,
    "A notification occurs after changing the prerender pref to false.");
  Assert.equal(aboutNewTabService.welcomeURL, aboutNewTabService.defaultURL,
    "Newtab welcomeURL is equal to defaultURL when prerendering disabled and debug disabled.");

  // Only debug variants aren't available on release/beta
  if (!IS_RELEASE_OR_BETA) {
    await setBoolPrefAndWaitForChange(ACTIVITY_STREAM_DEBUG_PREF, true,
      "A notification occurs after changing the debug pref to true.");
    Assert.equal(aboutNewTabService.welcomeURL, aboutNewTabService.welcomeURL,
      "Newtab welcomeURL is equal to defaultURL when prerendering disabled and debug enabled.");
    await setBoolPrefAndWaitForChange(ACTIVITY_STREAM_PRERENDER_PREF, true,
      "A notification occurs after changing the prerender pref to true.");
    Assert.equal(aboutNewTabService.welcomeURL, ACTIVITY_STREAM_DEBUG_URL,
      "Newtab welcomeURL set to un-prerendered debug AS when prerendering enabled and debug enabled");
  }

  cleanup();
});

add_task(function test_locale() {
  Assert.equal(aboutNewTabService.activityStreamLocale, "en-US",
    "The locale for testing should be en-US");
});

/**
 * Tests response to updates to prefs
 */
addTestsWithPrivilegedContentProcessPref(async function test_updates() {
  // Simulates a "cold-boot" situation, with some pref already set before testing a series
  // of changes.
  await setupASPrerendered();

  aboutNewTabService.resetNewTabURL(); // need to set manually because pref notifs are off
  let notificationPromise;

  // test update fires on override and reset
  let testURL = "https://example.com/";
  notificationPromise = nextChangeNotificationPromise(
    testURL, "a notification occurs on override");
  aboutNewTabService.newTabURL = testURL;
  await notificationPromise;

  // from overridden to default
  notificationPromise = nextChangeNotificationPromise(
    "about:newtab", "a notification occurs on reset");
  aboutNewTabService.resetNewTabURL();
  Assert.ok(aboutNewTabService.activityStreamEnabled, "Activity Stream should be enabled");
  Assert.equal(aboutNewTabService.defaultURL, ACTIVITY_STREAM_PRERENDER_URL, "Default URL should be the activity stream page");
  await notificationPromise;

  // reset twice, only one notification for default URL
  notificationPromise = nextChangeNotificationPromise(
    "about:newtab", "reset occurs");
  aboutNewTabService.resetNewTabURL();
  await notificationPromise;

  cleanup();
});
