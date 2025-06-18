/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This test ensures the placeholder is set correctly for different search
 * engines.
 */

"use strict";

const CONFIG = [
  {
    identifier: "defaultEngine",
    base: {
      urls: {
        // We need a default engine with trending results so the urlbar view will open.
        trending: {
          base: "https://example.com/browser/browser/components/search/test/browser/trendingSuggestionEngine.sjs",
          method: "GET",
        },
      },
    },
  },
  {
    identifier: "generalEngine",
    base: {
      classification: "general",
    },
  },
];

let appDefaultEngine, extraEngine, extraPrivateEngine, expectedString;
let tabs = [];

let noEngineString;
let keywordDisabledString;
SearchTestUtils.init(this);

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.scotchBonnet.enableOverride", false]],
  });
  let originalOrder = (await Services.search.getEngines()).map(e => e.id);
  await SearchTestUtils.updateRemoteSettingsConfig(CONFIG);
  appDefaultEngine = await Services.search.getDefault();
  [noEngineString, expectedString, keywordDisabledString] = (
    await document.l10n.formatMessages([
      { id: "urlbar-placeholder" },
      {
        id: "urlbar-placeholder-with-name",
        args: { name: appDefaultEngine.name },
      },
      { id: "urlbar-placeholder-keyword-disabled" },
    ])
  ).map(msg => msg.attributes[0].value);

  let rootUrl = getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://mochi.test:8888/"
  );
  await SearchTestUtils.installSearchExtension({
    name: "extraEngine",
    search_url: "https://mochi.test:8888/",
    suggest_url: `${rootUrl}/searchSuggestionEngine.sjs`,
  });
  extraEngine = Services.search.getEngineByName("extraEngine");
  await SearchTestUtils.installSearchExtension({
    name: "extraPrivateEngine",
    search_url: "https://mochi.test:8888/",
    suggest_url: `${rootUrl}/searchSuggestionEngine.sjs`,
  });
  extraPrivateEngine = Services.search.getEngineByName("extraPrivateEngine");

  // Force display of a tab with a URL bar, to clear out any possible placeholder
  // initialization listeners that happen on startup.
  let urlTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:mozilla"
  );
  BrowserTestUtils.removeTab(urlTab);

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.recentsearches.featureGate", false],
      ["browser.urlbar.trending.featureGate", true],
      ["browser.urlbar.suggest.searches", true],
      ["browser.urlbar.suggest.trending", true],
    ],
  });

  registerCleanupFunction(async () => {
    for (let tab of tabs) {
      BrowserTestUtils.removeTab(tab);
    }
    // At this point, the app provided engines have already been
    // restored by SearchTestUtils's cleanup but their order has not.
    for (let [index, id] of originalOrder.entries()) {
      let engine = Services.search.getEngineById(id);
      Services.search.moveEngine(engine, index);
    }
  });
});

add_task(async function test_change_default_engine_updates_placeholder() {
  tabs.push(await BrowserTestUtils.openNewForegroundTab(gBrowser));

  await Services.search.setDefault(
    extraEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  await TestUtils.waitForCondition(
    () => gURLBar.placeholder == noEngineString,
    "The placeholder should match the default placeholder for non-built-in engines."
  );
  Assert.equal(gURLBar.placeholder, noEngineString);

  await Services.search.setDefault(
    appDefaultEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  await TestUtils.waitForCondition(
    () => gURLBar.placeholder == expectedString,
    "The placeholder should include the engine name for built-in engines."
  );
  Assert.equal(gURLBar.placeholder, expectedString);
});

add_task(async function test_delayed_update_placeholder() {
  await doDelayedUpdatePlaceholderTest({ defaultEngine: extraEngine });
  await doDelayedUpdatePlaceholderTest({ defaultEngine: appDefaultEngine });
});

async function doDelayedUpdatePlaceholderTest({ defaultEngine }) {
  info("Set default search engine");
  await Services.search.setDefault(
    defaultEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  info("Clear placeholder cache");
  Services.prefs.clearUserPref("browser.urlbar.placeholderName");

  info("Open a new window");
  let newWin = await BrowserTestUtils.openNewBrowserWindow();

  Assert.equal(
    newWin.gURLBar.placeholder,
    noEngineString,
    "Placeholder should be unchanged."
  );
  Assert.deepEqual(
    newWin.document.l10n.getAttributes(newWin.gURLBar.inputField),
    { id: "urlbar-placeholder", args: null },
    "Placeholder data should be unchanged."
  );

  info("Simulate user interaction");
  let urlTab = BrowserTestUtils.addTab(newWin.gBrowser, "about:mozilla");
  await BrowserTestUtils.switchTab(newWin.gBrowser, urlTab);
  if (defaultEngine.isAppProvided) {
    await TestUtils.waitForCondition(
      () => newWin.gURLBar.placeholder == expectedString,
      "The placeholder should include the engine name for built-in engines."
    );
    Assert.ok(true, "Placeholder should be updated");
  } else {
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(r => setTimeout(r, 1000));
    Assert.equal(
      newWin.gURLBar.placeholder,
      noEngineString,
      "Placeholder should be unchanged."
    );
  }

  await BrowserTestUtils.closeWindow(newWin);
}

add_task(async function test_private_window_no_separate_engine() {
  const win = await BrowserTestUtils.openNewBrowserWindow({ private: true });

  await Services.search.setDefault(
    extraEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  await TestUtils.waitForCondition(
    () => win.gURLBar.placeholder == noEngineString,
    "The placeholder should match the default placeholder for non-built-in engines."
  );
  Assert.equal(win.gURLBar.placeholder, noEngineString);

  await Services.search.setDefault(
    appDefaultEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  await TestUtils.waitForCondition(
    () => win.gURLBar.placeholder == expectedString,
    "The placeholder should include the engine name for built-in engines."
  );
  Assert.equal(win.gURLBar.placeholder, expectedString);

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_private_window_separate_engine() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.separatePrivateDefault", true]],
  });
  const win = await BrowserTestUtils.openNewBrowserWindow({ private: true });

  // Keep the normal default as a different string to the private, so that we
  // can be sure we're testing the right thing.
  await Services.search.setDefault(
    appDefaultEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
  await Services.search.setDefaultPrivate(
    extraPrivateEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  await TestUtils.waitForCondition(
    () => win.gURLBar.placeholder == noEngineString,
    "The placeholder should match the default placeholder for non-built-in engines."
  );
  Assert.equal(win.gURLBar.placeholder, noEngineString);

  await Services.search.setDefault(
    extraEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
  await Services.search.setDefaultPrivate(
    appDefaultEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  await TestUtils.waitForCondition(
    () => win.gURLBar.placeholder == expectedString,
    "The placeholder should include the engine name for built-in engines."
  );
  Assert.equal(win.gURLBar.placeholder, expectedString);

  await BrowserTestUtils.closeWindow(win);

  // Verify that the placeholder for private windows is updated even when no
  // private window is visible (https://bugzilla.mozilla.org/1792816).
  await Services.search.setDefault(
    appDefaultEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
  await Services.search.setDefaultPrivate(
    extraPrivateEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
  const win2 = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  Assert.equal(win2.gURLBar.placeholder, noEngineString);
  await BrowserTestUtils.closeWindow(win2);

  // And ensure this doesn't affect the placeholder for non private windows.
  tabs.push(await BrowserTestUtils.openNewForegroundTab(gBrowser));
  Assert.equal(win.gURLBar.placeholder, expectedString);
});

add_task(async function test_search_mode_engine_web() {
  await Services.search.setDefault(
    appDefaultEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  await doSearchModeTest(
    {
      source: UrlbarUtils.RESULT_SOURCE.SEARCH,
      engineName: "generalEngine",
    },
    {
      id: "urlbar-placeholder-search-mode-web-2",
      args: { name: "generalEngine" },
    }
  );
});

add_task(async function test_search_mode_engine_other() {
  await doSearchModeTest(
    { engineName: extraEngine.name },
    {
      id: "urlbar-placeholder-search-mode-other-engine",
      args: { name: extraEngine.name },
    }
  );
});

add_task(async function test_search_mode_bookmarks() {
  await doSearchModeTest(
    { source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS },
    { id: "urlbar-placeholder-search-mode-other-bookmarks", args: null }
  );
});

add_task(async function test_search_mode_tabs() {
  await doSearchModeTest(
    { source: UrlbarUtils.RESULT_SOURCE.TABS },
    { id: "urlbar-placeholder-search-mode-other-tabs", args: null }
  );
});

add_task(async function test_search_mode_history() {
  await doSearchModeTest(
    { source: UrlbarUtils.RESULT_SOURCE.HISTORY },
    { id: "urlbar-placeholder-search-mode-other-history", args: null }
  );
});

add_task(async function test_change_default_engine_updates_placeholder() {
  tabs.push(await BrowserTestUtils.openNewForegroundTab(gBrowser));

  info(`Set engine to ${extraEngine.name}`);
  await Services.search.setDefault(
    extraEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
  await TestUtils.waitForCondition(
    () => gURLBar.placeholder == noEngineString,
    "The placeholder should match the default placeholder for non-built-in engines."
  );
  Assert.equal(gURLBar.placeholder, noEngineString);

  info(`Set engine to ${appDefaultEngine.name}`);
  await Services.search.setDefault(
    appDefaultEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
  await TestUtils.waitForCondition(
    () => gURLBar.placeholder == expectedString,
    "The placeholder should include the engine name for built-in engines."
  );

  // Simulate the placeholder not having changed due to the delayed update
  // on startup.
  gURLBar._setPlaceholder("");
  await TestUtils.waitForCondition(
    () => gURLBar.placeholder == noEngineString,
    "The placeholder should have been reset."
  );

  info("Show search engine removal info bar");
  BrowserUtils.callModulesFromCategory(
    { categoryName: "search-service-notification" },
    "search-engine-removal",
    extraEngine.name,
    appDefaultEngine.name
  );
  await TestUtils.waitForCondition(
    () => gNotificationBox.getNotificationWithValue("search-engine-removal"),
    "Waiting for message to be displayed"
  );
  const notificationBox = gNotificationBox.getNotificationWithValue(
    "search-engine-removal"
  );
  Assert.ok(notificationBox, "Search engine removal should be shown.");

  await TestUtils.waitForCondition(
    () => gURLBar.placeholder == expectedString,
    "The placeholder should include the engine name for built-in engines."
  );

  Assert.equal(gURLBar.placeholder, expectedString);

  notificationBox.close();
});

add_task(async function test_keyword_disabled() {
  tabs.push(await BrowserTestUtils.openNewForegroundTab(gBrowser));

  await SpecialPowers.pushPrefEnv({
    set: [["keyword.enabled", false]],
  });
  await TestUtils.waitForCondition(
    () => gURLBar.placeholder == keywordDisabledString
  );
  Assert.ok(true, "Updated the placeholder to the keyword disabled one.");

  await SpecialPowers.popPrefEnv();
  await TestUtils.waitForCondition(() => gURLBar.placeholder == expectedString);
  Assert.ok(true, "Updated the placeholder to the keyword enabled one.");
});

/**
 * Opens the view, clicks a one-off button to enter search mode, and asserts
 * that the placeholder is corrrect.
 *
 * @param {object} expectedSearchMode
 *   The expected search mode object for the one-off.
 * @param {object} expectedPlaceholderL10n
 *   The expected l10n object for the one-off.
 */
async function doSearchModeTest(expectedSearchMode, expectedPlaceholderL10n) {
  // Click the urlbar to open the top-sites view.
  if (gURLBar.getAttribute("pageproxystate") == "invalid") {
    gURLBar.handleRevert();
  }
  await UrlbarTestUtils.promisePopupOpen(window, () => {
    EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
  });

  // Enter search mode.
  await UrlbarTestUtils.enterSearchMode(window, expectedSearchMode);

  // Check the placeholder.
  Assert.deepEqual(
    document.l10n.getAttributes(gURLBar.inputField),
    expectedPlaceholderL10n,
    "Placeholder has expected l10n"
  );

  await UrlbarTestUtils.exitSearchMode(window, { clickClose: true });
  await UrlbarTestUtils.promisePopupClose(window);
}
