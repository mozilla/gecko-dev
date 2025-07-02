/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { RemoteSettings } = ChromeUtils.importESModule(
  "resource://services-settings/remote-settings.sys.mjs"
);

// Name of the RemoteSettings collection containing exceptions.
const COLLECTION_NAME = "url-classifier-exceptions";

// RemoteSettings collection db.
let db;

/**
 * Wait for the exception list service to initialize.
 * @param {string} [urlPattern] - The URL pattern to wait for to be present.
 * Pass null to wait for all entries to be removed.
 */
async function waitForExceptionListServiceSynced(urlPattern) {
  info(
    `Waiting for the exception list service to initialize for ${urlPattern}`
  );
  let classifier = Cc["@mozilla.org/url-classifier/dbservice;1"].getService(
    Ci.nsIURIClassifier
  );
  let feature = classifier.getFeatureByName("tracking-protection");
  await TestUtils.waitForCondition(() => {
    if (urlPattern == null) {
      return feature.exceptionList.testGetEntries().length === 0;
    }
    return feature.exceptionList
      .testGetEntries()
      .some(entry => entry.urlPattern === urlPattern);
  }, "Exception list service initialized");
}

/**
 * Dispatch a RemoteSettings "sync" event.
 * @param {Object} data - The event's data payload.
 * @param {Object} [data.created] - Records that were created.
 * @param {Object} [data.updated] - Records that were updated.
 * @param {Object} [data.deleted] - Records that were removed.
 * @param {Object} [data.current] - The current list of records.
 */
async function remoteSettingsSync({ created, updated, deleted, current }) {
  await RemoteSettings(COLLECTION_NAME).emit("sync", {
    data: {
      created,
      updated,
      deleted,
      // The list service seems to require this field to be set.
      current,
    },
  });
}

/**
 * Wait for a content blocking event to occur.
 * @param {Window} win - The window to listen for the event on.
 * @returns {Promise} A promise that resolves when the event occurs.
 */
async function waitForContentBlockingEvent(win) {
  return new Promise(resolve => {
    let listener = {
      onContentBlockingEvent(webProgress, request, event) {
        if (event & Ci.nsIWebProgressListener.STATE_BLOCKED_TRACKING_CONTENT) {
          win.gBrowser.removeProgressListener(listener);
          resolve();
        }
      },
    };
    win.gBrowser.addProgressListener(listener);
  });
}

/**
 * Load a tracker in third-party context.
 * @param {string} topLevelURL - The URL of the top level page to load.
 * @param {boolean} usePrivateWindow - Whether to use a private window.
 * @returns {Promise} A promise that resolves when the tracker is loaded or
 * blocked. The promise resolves to "loaded" if the tracker is loaded, or
 * "blocked" if it is blocked.
 */
async function loadTracker({ topLevelURL, usePrivateWindow }) {
  let win = window;

  if (usePrivateWindow) {
    win = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  }

  let tab = win.gBrowser.addTab(topLevelURL, {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });
  win.gBrowser.selectedTab = tab;
  let browser = tab.linkedBrowser;

  await BrowserTestUtils.browserLoaded(browser, false, topLevelURL);

  info("Create an iframe loading tracking.example.org.");

  let blockedPromise = waitForContentBlockingEvent(win).then(() => "blocked");

  let loadPromise = SpecialPowers.spawn(
    browser,
    ["https://tracking.example.org"],
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
  if (usePrivateWindow) {
    await BrowserTestUtils.closeWindow(win);
  } else {
    await BrowserTestUtils.removeTab(tab);
  }

  return result == "loaded";
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Add test tracker.
      ["urlclassifier.trackingTable.testEntries", "tracking.example.org"],
      [
        "urlclassifier.trackingAnnotationTable.testEntries",
        "tracking.example.org",
      ],
      // Clear predefined exceptions.
      ["urlclassifier.trackingAnnotationSkipURLs", ""],
      ["urlclassifier.trackingSkipURLs", ""],
      // Make sure we're in "standard" mode.
      ["browser.contentblocking.category", "standard"],
    ],
  });

  // Start with an empty RS collection.
  info(`Initializing RemoteSettings collection "${COLLECTION_NAME}".`);
  db = RemoteSettings(COLLECTION_NAME).db;
  await db.importChanges({}, Date.now(), [], { clear: true });
});

/**
 * Set exceptions via RemoteSettings.
 * @param {Object[]} entries - The entries to set. If empty, the exceptions will be cleared.
 */
async function setExceptions(entries) {
  info("Set exceptions via RemoteSettings");
  if (!entries.length) {
    await db.clear();
    await db.importChanges({}, Date.now());
    await remoteSettingsSync({ current: [] });
    await waitForExceptionListServiceSynced();
    return;
  }

  let entriesPromises = entries.map(e =>
    db.create({
      category: "baseline",
      urlPattern: e.urlPattern,
      classifierFeatures: e.classifierFeatures,
      // Only apply to private browsing in ETP "standard" mode.
      isPrivateBrowsingOnly: e.isPrivateBrowsingOnly,
      filterContentBlockingCategories: e.filterContentBlockingCategories,
    })
  );

  let rsEntries = await Promise.all(entriesPromises);

  await db.importChanges({}, Date.now());
  await remoteSettingsSync({ current: rsEntries });
  await waitForExceptionListServiceSynced(rsEntries[0].urlPattern);
}

// Tests that exception list entries can be scoped to only private browsing.
add_task(async function test_private_browsing_exception() {
  info("Load tracker in normal browsing.");
  let success = await loadTracker({
    topLevelURL: "https://example.com/",
    usePrivateWindow: false,
  });
  is(
    success,
    true,
    "Tracker access should be allowed in normal browsing, because TP is disabled."
  );

  info("Load tracker in private browsing.");
  success = await loadTracker({
    topLevelURL: "https://example.com/",
    usePrivateWindow: true,
  });
  is(
    success,
    false,
    "Tracker access should be blocked in private browsing, because TP is enabled and there is no exception set."
  );

  info("Set exception for private browsing.");
  await setExceptions([
    {
      urlPattern: "*://tracking.example.org/*",
      topLevelUrlPattern: "*://example.com/*",
      classifierFeatures: ["tracking-protection"],
      isPrivateBrowsingOnly: true,
      filterContentBlockingCategories: ["standard"],
    },
  ]);

  info("Load tracker in normal browsing.");
  success = await loadTracker({
    topLevelURL: "https://example.com/",
    usePrivateWindow: false,
  });
  is(
    success,
    true,
    "Tracker access should be allowed in normal browsing, because TP is disabled."
  );

  info("Load tracker in private browsing.");
  success = await loadTracker({
    topLevelURL: "https://example.com/",
    usePrivateWindow: true,
  });
  is(
    success,
    true,
    "Tracker access should be allowed in private browsing even though TP is enabled, because it's allow-listed."
  );

  info("Switch to ETP 'strict' mode.");
  await SpecialPowers.pushPrefEnv({
    set: [["browser.contentblocking.category", "strict"]],
  });

  info("Load tracker in normal browsing.");
  success = await loadTracker({
    topLevelURL: "https://example.com/",
    usePrivateWindow: false,
  });
  is(
    success,
    false,
    "Tracker access should be blocked in normal browsing in ETP 'strict' mode."
  );

  info("Load tracker in private browsing.");
  success = await loadTracker({
    topLevelURL: "https://example.com/",
    usePrivateWindow: true,
  });
  is(
    success,
    false,
    "Tracker access should be blocked in private browsing in ETP 'strict' mode."
  );

  info("Switch back to ETP 'standard' mode.");
  await SpecialPowers.popPrefEnv();

  info("Test with different top level site.");
  success = await loadTracker({
    topLevelURL: "https://example.org/",
    usePrivateWindow: false,
  });
  is(
    success,
    true,
    "Tracker access should be allowed in normal browsing, because TP is disabled."
  );

  info("Load tracker in private browsing.");
  success = await loadTracker({
    topLevelURL: "https://example.org/",
    usePrivateWindow: true,
  });
  is(
    success,
    true,
    "Tracker access should be blocked in private browsing, because TP is enabled and there is no exception set for top the top level URL https://example.org."
  );

  info("Update exception, removing the additional filtering criteria.");
  await setExceptions([
    {
      urlPattern: "*://tracking.example.org/*",
      classifierFeatures: ["tracking-protection"],
      filterContentBlockingCategories: ["standard", "strict"],
    },
  ]);

  info("Load tracker in private browsing.");
  success = await loadTracker({
    topLevelURL: "https://example.org/",
    usePrivateWindow: true,
  });
  is(
    success,
    true,
    "Tracker access should be allowed in private browsing, because TP is enabled and there is an exception set."
  );

  info("Switch to ETP 'strict' mode.");
  await SpecialPowers.pushPrefEnv({
    set: [["browser.contentblocking.category", "strict"]],
  });

  info("Load tracker in normal browsing.");
  success = await loadTracker({
    topLevelURL: "https://example.com/",
    usePrivateWindow: false,
  });
  is(
    success,
    true,
    "Tracker access should be allowed in normal browsing in ETP 'strict' mode, because there is an exception set."
  );

  info("Cleanup");
  await SpecialPowers.popPrefEnv();
  await setExceptions([]);
});
