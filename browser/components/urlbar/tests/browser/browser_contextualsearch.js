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
    identifier: "config-engine",
    base: {
      urls: {
        search: { base: "https://example.org", searchTermParamName: "q" },
      },
    },
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

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.contextualSearch.enabled", true],
      ["browser.urlbar.scotchBonnet.enableOverride", true],
    ],
  });

  let ext = await SearchTestUtils.installSearchExtension({
    name: "Contextual",
    search_url: "https://example.com/browser",
  });
  await AddonTestUtils.waitForSearchProviderStartup(ext);
});

add_task(async function test_no_engine() {
  const ENGINE_TEST_URL = "https://example.org/";
  let onLoaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    ENGINE_TEST_URL
  );
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    ENGINE_TEST_URL
  );
  await onLoaded;

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
  });

  Assert.ok(
    UrlbarTestUtils.getResultCount(window) > 0,
    "At least one result is shown"
  );
});

add_task(async function test_selectContextualSearchResult_already_installed() {
  const ENGINE_TEST_URL = "https://example.com/";
  let onLoaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    ENGINE_TEST_URL
  );
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    ENGINE_TEST_URL
  );
  await onLoaded;

  const query = "search";
  let engine = Services.search.getEngineByName("Contextual");
  const [expectedUrl] = UrlbarUtils.getSearchQueryUrl(engine, query);

  Assert.ok(
    expectedUrl.includes(`?q=${query}`),
    "Expected URL should be a search URL"
  );

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: query,
  });

  info("Focus and select the contextual search result");
  let onLoad = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    expectedUrl
  );

  EventUtils.synthesizeKey("KEY_Tab");
  EventUtils.synthesizeKey("KEY_Enter");
  await onLoad;

  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: "Contextual",
    entry: "other",
  });
  Assert.equal(
    gBrowser.selectedBrowser.currentURI.spec,
    expectedUrl,
    "Selecting the contextual search result opens the search URL"
  );
  window.document.querySelector("#searchmode-switcher-close").click();
});

add_task(async function test_selectContextualSearchResult_not_installed() {
  const ENGINE_TEST_URL =
    "http://mochi.test:8888/browser/browser/components/search/test/browser/opensearch.html";
  const EXPECTED_URL =
    "http://mochi.test:8888/browser/browser/components/search/test/browser/?search&test=search";
  let onLoaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    ENGINE_TEST_URL
  );
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    ENGINE_TEST_URL
  );
  await onLoaded;

  const query = "search";

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: query,
  });

  await UrlbarTestUtils.promiseSearchComplete(window);

  Assert.ok(
    !Services.search.getEngineByName("Foo"),
    "Engine is not currently installed."
  );

  info("Focus and select the contextual search result");
  let onLoad = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    EXPECTED_URL
  );
  EventUtils.synthesizeKey("KEY_Tab");
  EventUtils.synthesizeKey("KEY_Enter");
  await onLoad;

  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: "Foo",
    entry: "other",
  });

  Assert.equal(
    gBrowser.selectedBrowser.currentURI.spec,
    EXPECTED_URL,
    "Selecting the contextual search result opens the search URL"
  );

  let engine = Services.search.getEngineByName("Foo");
  Assert.ok(engine != null, "Engine was installed.");
  Assert.equal(
    engine.wrappedJSObject.getAttr("auto-installed"),
    true,
    "Engine was marks as auto installed."
  );

  window.document.querySelector("#searchmode-switcher-close").click();
  await UrlbarTestUtils.promisePopupClose(window);

  await Services.search.removeEngine(engine);
  ActionsProviderContextualSearch.resetForTesting();
});

add_task(async function test_contextual_search_engine() {
  await waitForIdle();
  SearchTestUtils.setRemoteSettingsConfig(CONFIG);
  await Services.search.wrappedJSObject.reset();
  await Services.search.init();
  await loadUri("https://example.org/");

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
  });

  let expectedUrl = "https://example.org/?q=test";
  info("Focus and select the contextual search result");
  let onLoad = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    expectedUrl
  );

  EventUtils.synthesizeKey("KEY_Tab");
  EventUtils.synthesizeKey("KEY_Enter");
  await onLoad;

  Assert.equal(
    gBrowser.selectedBrowser.currentURI.spec,
    expectedUrl,
    "Selecting the contextual search result opens the search URL"
  );

  let engine = Services.search.getEngineByName("config-engine");
  Assert.ok(engine != null, "Engine was installed.");
  Assert.equal(engine.name, "config-engine", "Correct engine was installed.");

  window.document.querySelector("#searchmode-switcher-close").click();
  await UrlbarTestUtils.promisePopupClose(window);

  await Services.search.wrappedJSObject.reset();
  await Services.search.init();

  ActionsProviderContextualSearch.resetForTesting();
});

async function waitForIdle() {
  for (let i = 0; i < 10; i++) {
    await new Promise(resolve => Services.tm.idleDispatchToMainThread(resolve));
  }
}
