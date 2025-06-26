/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

const { ActionsProviderQuickActions } = ChromeUtils.importESModule(
  "resource:///modules/ActionsProviderQuickActions.sys.mjs"
);

const CONFIG = [
  {
    identifier: "default-engine",
    base: {
      urls: {
        search: { base: "https://example.com", searchTermParamName: "q" },
      },
    },
  },
  {
    identifier: "non-default-engine",
    base: {
      urls: {
        search: { base: "https://example.net", searchTermParamName: "q" },
      },
    },
  },

  {
    identifier: "config-engine",
    base: {
      urls: {
        search: { base: "https://example.org", searchTermParamName: "q" },
      },
    },
    // Only enable in particular locale so it is not installed by default.
    variants: [{ environment: { locales: ["sl"] } }],
  },
];

let loadUri = async uri => {
  let loaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    uri
  );
  BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, uri);
  await loaded;
};

let updateConfig = async config => {
  await waitForIdle();
  await SearchTestUtils.setRemoteSettingsConfig(config);
  await Services.search.wrappedJSObject.reset();
  await Services.search.init();
};

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.scotchBonnet.enableOverride", true]],
  });

  registerCleanupFunction(async () => {
    await updateConfig(null);
    Services.search.restoreDefaultEngines();
  });
});

add_task(async function test_no_engine() {
  await loadUri("https://example.org/");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
  });

  Assert.ok(
    UrlbarTestUtils.getResultCount(window) > 0,
    "At least one result is shown"
  );
  await UrlbarTestUtils.promisePopupClose(window);
});

add_task(async function test_engine_match() {
  let promiseClearHistory =
    PlacesTestUtils.waitForNotification("history-cleared");
  await PlacesUtils.history.clear();
  await promiseClearHistory;
  await updateConfig(CONFIG);
  await loadUri("https://example.org/");

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "non",
  });

  Assert.ok(
    !(await hasActions(1)),
    "Contextual result does not match because site has not been visited"
  );
  await UrlbarTestUtils.promisePopupClose(window, () => {
    gURLBar.blur();
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.net/"
  );
  BrowserTestUtils.removeTab(tab);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "non",
  });

  Assert.ok(await hasActions(1), "Contextual search is matched after visit");

  let onLoad = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    "https://example.net/?q=test"
  );
  let btn = window.document.querySelector(".urlbarView-action-btn");
  EventUtils.synthesizeMouseAtCenter(btn, {}, window);
  EventUtils.sendString("test");
  EventUtils.synthesizeKey("KEY_Enter");

  await onLoad;
  await updateConfig(null);
});

add_task(async function test_actions() {
  let testActionCalled = 0;
  await updateConfig(CONFIG);
  await loadUri("https://example.net/");

  ActionsProviderQuickActions.addAction("testaction", {
    commands: ["example"],
    label: "quickactions-downloads2",
    onPick: () => testActionCalled++,
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "example.net",
  });

  EventUtils.synthesizeKey("KEY_Tab");
  EventUtils.synthesizeKey("KEY_Tab");
  EventUtils.synthesizeKey("KEY_Enter");
  await UrlbarTestUtils.promisePopupClose(window);

  Assert.equal(testActionCalled, 1, "Test action was called");

  await updateConfig(null);
  ActionsProviderQuickActions.removeAction("testaction");
});

add_task(async function test_selectContextualSearchResult_already_installed() {
  let ext = await SearchTestUtils.installSearchExtension({
    name: "Contextual",
    search_url: "https://example.com/browser",
  });
  await AddonTestUtils.waitForSearchProviderStartup(ext);

  await loadUri("https://example.com/");

  const query = "search";
  let engine = Services.search.getEngineByName("Contextual");
  const [expectedUrl] = UrlbarUtils.getSearchQueryUrl(engine, query);

  Assert.ok(
    expectedUrl.includes(`?q=${query}`),
    "Expected URL should be a search URL"
  );

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "contextual",
  });

  let result = (await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1))
    .result;
  Assert.equal(
    result.providerName,
    "UrlbarProviderGlobalActions",
    "We are shown contextual search action"
  );
  info("Focus and select the contextual search result");
  EventUtils.synthesizeKey("KEY_Tab");
  EventUtils.synthesizeKey("KEY_Enter");
  await UrlbarTestUtils.promisePopupClose(window);

  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: "Contextual",
    entry: "keywordoffer",
  });

  let onLoad = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    expectedUrl
  );

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: query,
  });
  EventUtils.synthesizeKey("KEY_Enter");
  await onLoad;

  Assert.equal(
    gBrowser.selectedBrowser.currentURI.spec,
    expectedUrl,
    "Selecting the contextual search result opens the search URL"
  );
  await UrlbarTestUtils.exitSearchMode(window, {
    clickClose: true,
    waitForSearch: false,
  });
});

add_task(async function test_tab_to_search_engine() {
  let newConfig = [CONFIG[0]].concat([
    {
      identifier: "namematch-engine",
      base: {
        urls: {
          search: { base: "https://example.net", searchTermParamName: "q" },
        },
      },
    },
  ]);
  await updateConfig(newConfig);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "namematch",
  });

  let onLoad = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    "https://example.net/?q=test"
  );

  EventUtils.synthesizeKey("KEY_Tab");
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: "namematch-engine",
    entry: "keywordoffer",
    isPreview: true,
    source: 3,
  });

  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.sendString("test");
    EventUtils.synthesizeKey("KEY_Enter");
  });

  await onLoad;
  await updateConfig(null);
});

async function hasActions(index) {
  if (UrlbarTestUtils.getResultCount(window) <= index) {
    return false;
  }
  let result = (await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1))
    .result;
  return result.providerName == "UrlbarProviderGlobalActions";
}

async function waitForIdle() {
  for (let i = 0; i < 10; i++) {
    await new Promise(resolve => Services.tm.idleDispatchToMainThread(resolve));
  }
}
