/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ActionsProviderContextualSearch } = ChromeUtils.importESModule(
  "resource:///modules/ActionsProviderContextualSearch.sys.mjs"
);

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
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
});

add_task(async function test_engine_match() {
  await updateConfig(CONFIG);
  await loadUri("https://example.org/");

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "example.net",
  });

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
  await updateConfig(CONFIG);
  await loadUri("https://example.org/");

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "example.ne",
  });

  let expectedUrl = "https://example.net/?q=test";
  info("Focus and select the contextual search result");
  let onLoad = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    expectedUrl
  );

  EventUtils.synthesizeKey("KEY_Tab");

  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: "non-default-engine",
    entry: "keywordoffer",
    isPreview: true,
    source: 3,
  });

  EventUtils.sendString("test");
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
  await updateConfig(null);
});

async function waitForIdle() {
  for (let i = 0; i < 10; i++) {
    await new Promise(resolve => Services.tm.idleDispatchToMainThread(resolve));
  }
}
