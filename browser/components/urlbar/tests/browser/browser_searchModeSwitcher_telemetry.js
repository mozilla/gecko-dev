/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.scotchBonnet.enableOverride", true]],
  });
  await TestUtils.waitForCondition(
    () =>
      BrowserTestUtils.isVisible(
        document.getElementById("urlbar-searchmode-switcher")
      ),
    "search mode switcher button is visible"
  );

  registerCleanupFunction(async () => {
    await cleanUp();
  });
});

add_task(async function test_opened() {
  await cleanUp();

  info("Open search mode switcher popup");
  await UrlbarTestUtils.openSearchModeSwitcher(window);
  Assert.equal(Glean.urlbarUnifiedsearchbutton.opened.testGetValue(), 1);

  info("Close search mode switcher popup");
  EventUtils.synthesizeKey("KEY_Escape", {});

  info("Open search mode switcher popup again");
  await UrlbarTestUtils.openSearchModeSwitcher(window);
  Assert.equal(Glean.urlbarUnifiedsearchbutton.opened.testGetValue(), 2);

  info("Close search mode switcher popup again");
  EventUtils.synthesizeKey("KEY_Escape", {});
});

add_task(async function test_picked_search_engines() {
  await cleanUp();

  info("Open a new tab");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  info("Start search engine tests");
  await testSearchEngine("Google", "builtin_search", 1);
  await testSearchEngine("Google", "builtin_search", 2);
  await testSearchEngine("DuckDuckGo", "builtin_search", 3);
  await testSearchEngine("Bookmarks", "local_search", 1);
  await testSearchEngine("Tabs", "local_search", 2);
  await testSearchEngine("DuckDuckGo", "builtin_search", 4);
  await testSearchEngine("Bookmarks", "local_search", 3);
  await testSearchEngine("Tabs", "local_search", 4);
  await testSearchEngine("DuckDuckGo", "builtin_search", 5);

  info("Add addon search engine");
  await loadUri(
    "http://mochi.test:8888/browser/browser/components/search/test/browser/opensearch.html"
  );
  info("Ensure to show Unified Search Button");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus: true,
    value: "",
    fireInputEvent: true,
  });

  info("Test with addon search engine");
  await testSearchEngine("engine1", "addon_search", 1);
  await testSearchEngine("Foo", "addon_search", 2);
  await testSearchEngine("DuckDuckGo", "builtin_search", 6);
  await testSearchEngine("Bookmarks", "local_search", 5);

  info("Clean up");
  await removeAddonSearchEngine("Foo");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_picked_settings() {
  await cleanUp();
  Assert.equal(
    Glean.urlbarUnifiedsearchbutton.picked.settings.testGetValue(),
    null
  );

  info("Open a new tab");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  let popup = await UrlbarTestUtils.openSearchModeSwitcher(window);
  let popupHidden = UrlbarTestUtils.searchModeSwitcherPopupClosed(window);
  let pageLoaded = BrowserTestUtils.browserLoaded(window);
  popup
    .querySelector("#searchmode-switcher-popup-search-settings-button")
    .click();
  await Promise.all([pageLoaded, popupHidden]);
  Assert.equal(
    Glean.urlbarUnifiedsearchbutton.picked.settings.testGetValue(),
    1
  );

  BrowserTestUtils.removeTab(tab);
});

async function testSearchEngine(label, telemetry, expected) {
  info(
    `Test search engine for ${JSON.stringify({ label, telemetry, expected })}`
  );
  let popup = await UrlbarTestUtils.openSearchModeSwitcher(window);

  let popupHidden = UrlbarTestUtils.searchModeSwitcherPopupClosed(window);
  await BrowserTestUtils.waitForCondition(() =>
    popup.querySelector(`toolbarbutton[label=${label}]`)
  );
  popup.querySelector(`toolbarbutton[label=${label}]`).click();
  await popupHidden;
  Assert.equal(
    Glean.urlbarUnifiedsearchbutton.picked[telemetry].testGetValue(),
    expected
  );

  document.querySelector("#searchmode-switcher-close").click();
}

async function loadUri(uri) {
  let loaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    uri
  );
  BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, uri);
  await loaded;
}

async function removeAddonSearchEngine(name) {
  let promiseEngineRemoved = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.REMOVED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
  let settingsWritten = SearchTestUtils.promiseSearchNotification(
    "write-settings-to-disk-complete"
  );
  let engine = Services.search.getEngineByName(name);
  await Promise.all([
    Services.search.removeEngine(engine),
    promiseEngineRemoved,
    settingsWritten,
  ]);
}

async function cleanUp() {
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();
  Assert.equal(Glean.urlbarUnifiedsearchbutton.opened.testGetValue(), null);
}
