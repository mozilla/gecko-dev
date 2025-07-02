/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Name of the RemoteSettings collection containing exceptions.
const COLLECTION_NAME = "url-classifier-exceptions";

/**
 * Load a tracker in third-party context.
 * @param {string} trackerUrl - The URL of the tracker to load in an iframe.
 * @returns {Promise} A promise that resolves when the tracker is loaded or
 * blocked. The promise resolves to "loaded" if the tracker is loaded, or
 * "blocked" if it is blocked.
 */
async function loadTracker({ trackerUrl }) {
  let win = window;
  const topLevelURL = "https://example.com/";
  let tab = win.gBrowser.addTab(topLevelURL, {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });
  win.gBrowser.selectedTab = tab;
  let browser = tab.linkedBrowser;

  await BrowserTestUtils.browserLoaded(browser, false, topLevelURL);

  info(`Create an iframe loading ${trackerUrl}`);

  let blockedPromise = waitForContentBlockingEvent(win).then(() => "blocked");

  let loadPromise = SpecialPowers.spawn(
    browser,
    [trackerUrl],
    async function (url) {
      let iframe = content.document.createElement("iframe");
      let loadPromise = ContentTaskUtils.waitForEvent(iframe, "load").then(
        () => "loaded"
      );
      iframe.src = url;
      content.document.body.appendChild(iframe);
      return loadPromise;
    }
  );

  let result = await Promise.race([loadPromise, blockedPromise]);

  // Clean up.
  await BrowserTestUtils.removeTab(tab);

  return result == "loaded";
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Add test tracker.
      [
        "urlclassifier.trackingTable.testEntries",
        "tracking.example.org,email-tracking.example.org",
      ],
      // Clear predefined exceptions.
      ["urlclassifier.trackingAnnotationSkipURLs", ""],
      ["urlclassifier.trackingSkipURLs", ""],
      ["browser.contentblocking.category", "strict"],
    ],
  });

  // Start with an empty RS collection.
  info(`Initializing RemoteSettings collection "${COLLECTION_NAME}".`);
  const db = RemoteSettings(COLLECTION_NAME).db;
  await db.importChanges({}, Date.now(), [], { clear: true });
  await setExceptions(
    [
      {
        urlPattern: "*://email-tracking.example.org/*",
        classifierFeatures: ["tracking-protection"],
        category: "baseline",
      },
      {
        urlPattern: "*://tracking.example.org/*",
        classifierFeatures: ["tracking-protection"],
        category: "convenience",
      },
    ],
    db,
    COLLECTION_NAME
  );

  registerCleanupFunction(async () => {
    info("Cleanup RemoteSettings collection and preferences");
    await setExceptions([], db, COLLECTION_NAME);
    await SpecialPowers.flushPrefEnv();
  });
});

/**
 * Set exceptions via RemoteSettings.
 * @param {Boolean} baseline - If true, set baseline allow list entries.
 * @param {Boolean} convenience - If true, set convenience allow list entries.
 */
async function setAllowListPrefs(baseline, convenience) {
  info("Set allow list baseline and convenience prefs");
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.trackingprotection.allow_list.baseline.enabled", baseline],
      [
        "privacy.trackingprotection.allow_list.convenience.enabled",
        convenience,
      ],
    ],
  });
}

add_task(async function test_allow_list_both_enabled() {
  info("Set baseline to true and convenience to true.");
  await setAllowListPrefs(true, true);

  let success = await loadTracker({
    trackerUrl: "https://email-tracking.example.org/",
  });
  is(
    success,
    true,
    "Tracker access should be allowed because baseline allow list is enabled."
  );

  success = await loadTracker({
    trackerUrl: "https://tracking.example.org/",
  });
  is(
    success,
    true,
    "Tracker access should be allowed because convenience allow list is enabled."
  );
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_allow_list_baseline_only() {
  info("Set baseline to true and convenience to false.");
  await setAllowListPrefs(true, false);

  let success = await loadTracker({
    trackerUrl: "https://email-tracking.example.org/",
  });
  is(
    success,
    true,
    "Tracker access should be allowed because baseline allow list is enabled."
  );

  success = await loadTracker({
    trackerUrl: "https://tracking.example.org/",
  });
  is(
    success,
    false,
    "Tracker access should be blocked because convenience allow list is disabled."
  );
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_allow_list_none_enabled() {
  info("Set baseline to false and convenience to false.");
  await setAllowListPrefs(false, false);
  console.log(
    Services.prefs.getBoolPref(
      "privacy.trackingprotection.allow_list.baseline.enabled"
    )
  );
  console.log(
    Services.prefs.getBoolPref(
      "privacy.trackingprotection.allow_list.convenience.enabled"
    )
  );

  let success = await loadTracker({
    trackerUrl: "https://email-tracking.example.org/",
  });
  is(
    success,
    false,
    "Tracker access should be blocked because baseline allow lists is disabled."
  );

  success = await loadTracker({
    trackerUrl: "https://tracking.example.org/",
  });
  is(
    success,
    false,
    "Tracker access should be blocked because convenience allow list is disabled."
  );
  await SpecialPowers.popPrefEnv();
});
