/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Basic tests for secondary Actions.
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ActionsProviderQuickActions:
    "resource:///modules/ActionsProviderQuickActions.sys.mjs",
});

let testActionCalled = 0;

let loadURI = async (browser, uri) => {
  let onLoaded = BrowserTestUtils.browserLoaded(browser, false, uri);
  BrowserTestUtils.startLoadingURIString(browser, uri);
  return onLoaded;
};

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.scotchBonnet.enableOverride", true]],
  });

  ActionsProviderQuickActions.addAction("testaction", {
    commands: ["testaction"],
    actionKey: "testaction",
    label: "quickactions-downloads2",
    onPick: () => testActionCalled++,
  });

  registerCleanupFunction(() => {
    ActionsProviderQuickActions.removeAction("testaction");
  });
});

add_task(async function test_quickaction() {
  info("Match an installed quickaction and trigger it via tab");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "testact",
  });

  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    2,
    "We matched the action"
  );

  info("The callback of the action is fired when selected");
  EventUtils.synthesizeKey("KEY_Tab", {}, window);
  EventUtils.synthesizeKey("KEY_Enter", {}, window);
  Assert.equal(testActionCalled, 1, "Test action was called");
});

add_task(async function test_switchtab() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.secondaryActions.switchToTab", true]],
  });

  let win = await BrowserTestUtils.openNewBrowserWindow();
  await loadURI(win.gBrowser, "https://example.com/");

  info("Open a new tab, type in the urlbar and switch to previous tab");
  await BrowserTestUtils.openNewForegroundTab(win.gBrowser);
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    value: "example",
  });

  EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
  EventUtils.synthesizeKey("KEY_Tab", {}, win);
  EventUtils.synthesizeKey("KEY_Enter", {}, win);

  is(win.gBrowser.tabs.length, 1, "We switched to previous tab");
  is(
    win.gBrowser.currentURI.spec,
    "https://example.com/",
    "We switched to previous tab"
  );

  info(
    "Open a new tab, type in the urlbar, select result and open url in current tab"
  );
  await BrowserTestUtils.openNewForegroundTab(win.gBrowser);
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    value: "example",
  });

  EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
  EventUtils.synthesizeKey("KEY_Enter", {}, win);
  await BrowserTestUtils.browserLoaded(win.gBrowser);
  is(win.gBrowser.tabs.length, 2, "We switched to previous tab");
  is(
    win.gBrowser.currentURI.spec,
    "https://example.com/",
    "We opened in current tab"
  );

  BrowserTestUtils.closeWindow(win);
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_switchtab_with_userContextId() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.secondaryActions.switchToTab", true]],
  });
  let url = "https://example.com";
  let tab = BrowserTestUtils.addTab(gBrowser, url, { userContextId: 1 });
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  info("Start query");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "exa",
  });

  info("Check the button style");
  let button = document.querySelector(
    ".urlbarView-actions-container .urlbarView-action-btn.urlbarView-userContext"
  );
  await BrowserTestUtils.waitForCondition(() => button.textContent.length);

  Assert.ok(button, "Action button with userContext is in the result");
  Assert.ok(button.textContent.includes("personal"), "Label is correct");
  Assert.ok(
    button.classList.contains("identity-color-blue"),
    "Style is correct"
  );

  info("Switch the tab");
  EventUtils.synthesizeMouseAtCenter(button, {});
  await BrowserTestUtils.waitForCondition(() => gBrowser.selectedTab == tab);
  Assert.ok(true, "Expected tab is selected");
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_sitesearch() {
  await SearchTestUtils.installSearchExtension({
    name: "Contextual",
    search_url: "https://example.com/browser",
  });

  let ENGINE_TEST_URL = "https://example.com/";
  await loadURI(gBrowser.selectedBrowser, ENGINE_TEST_URL);

  const query = "search";
  let engine = Services.search.getEngineByName("Contextual");
  const [expectedUrl] = UrlbarUtils.getSearchQueryUrl(engine, query);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "sea",
  });

  let onLoad = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    expectedUrl
  );
  EventUtils.synthesizeKey("KEY_Tab");
  EventUtils.sendString(query);
  EventUtils.synthesizeKey("KEY_Enter");
  await onLoad;

  Assert.equal(
    gBrowser.selectedBrowser.currentURI.spec,
    expectedUrl,
    "Selecting the contextual search result opens the search URL"
  );
});

add_task(async function enter_action_search_mode() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "> ",
  });
  await UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.ACTIONS,
    entry: "typed",
    restrictType: "symbol",
  });
  let { result } = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(
    result.providerName,
    "UrlbarProviderActionsSearchMode",
    "Actions are shown"
  );

  let pageLoaded = BrowserTestUtils.browserLoaded(window);
  EventUtils.synthesizeKey("pref", {}, window);
  EventUtils.synthesizeKey("KEY_Tab");
  EventUtils.synthesizeKey("KEY_Enter");
  await pageLoaded;

  Assert.equal(
    window.gBrowser.selectedBrowser.currentURI.spec,
    "about:preferences",
    "Opened settings page"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_opensearch() {
  const TEST_DATA = [
    {
      query: "word",
      expectedUrl: "http://mochi.test:8888/?terms=word",
    },
    {
      query: "word1 word2",
      expectedUrl: "http://mochi.test:8888/?terms=word1+word2",
    },
    {
      query: "https://example.com/",
      expectedUrl: "http://mochi.test:8888/?terms=https%3A%2F%2Fexample.com%2F",
    },
  ];
  for (let { query, expectedUrl } of TEST_DATA) {
    let url = getRootDirectory(gTestPath) + "add_search_engine_one.html";
    await BrowserTestUtils.withNewTab(url, async () => {
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: query,
      });
      let { result } = await UrlbarTestUtils.getRowAt(window, 1);
      Assert.equal(result.providerName, "UrlbarProviderGlobalActions");

      let onLoad = BrowserTestUtils.browserLoaded(
        gBrowser.selectedBrowser,
        false,
        expectedUrl
      );
      EventUtils.synthesizeKey("KEY_Tab");
      EventUtils.synthesizeKey("KEY_Enter");
      await onLoad;
      Assert.ok(true, "Action for open search works expectedly");
    });
  }
});
