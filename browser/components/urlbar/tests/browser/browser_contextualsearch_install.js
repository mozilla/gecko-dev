/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const ENGINE_TEST_URL =
  "http://mochi.test:8888/browser/browser/components/search/test/browser/opensearch.html";
const EXPECTED_URL =
  "http://mochi.test:8888/browser/browser/components/search/test/browser/?search&test=";
const NOTIFICATION_VALUE = "install-search-engine";

let loadUri = async uri => {
  let loaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    uri
  );
  BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, uri);
  await loaded;
};

let performContextualSearch = async query => {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: query,
  });

  let onLoad = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    EXPECTED_URL + query
  );
  EventUtils.synthesizeKey("KEY_Tab");
  EventUtils.synthesizeKey("KEY_Enter");
  await onLoad;
};

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.contextualSearch.enabled", true],
      ["browser.urlbar.scotchBonnet.enableOverride", true],
    ],
  });
});

add_task(async function test_contextualsearch_install_deny() {
  await loadUri(ENGINE_TEST_URL);
  await performContextualSearch("search");

  info("The first search only performs a search");
  await UrlbarTestUtils.assertSearchMode(window, null);

  let notificationBar =
    window.gNotificationBox.getNotificationWithValue(NOTIFICATION_VALUE);
  Assert.ok(!notificationBar, "No notification is shown");

  let notificationPromise = BrowserTestUtils.waitForGlobalNotificationBar(
    window,
    NOTIFICATION_VALUE
  );

  await loadUri(ENGINE_TEST_URL);
  await performContextualSearch("newsearch");

  let notification = await notificationPromise;
  notification.buttonContainer
    .querySelector("[data-l10n-id=install-search-engine-no]")
    .click();

  await loadUri(ENGINE_TEST_URL);
  await performContextualSearch("thirdsearch");

  notificationBar =
    window.gNotificationBox.getNotificationWithValue(NOTIFICATION_VALUE);
  Assert.ok(!notificationBar, "No notification is not shown after deny chosen");

  Services.search.wrappedJSObject._settings.setMetaDataAttribute(
    "contextual-engines-seen",
    {}
  );
});

add_task(async function test_contextualsearch_install() {
  let initialEngines = await Services.search.getVisibleEngines();
  await loadUri(ENGINE_TEST_URL);
  await performContextualSearch("search");

  info("The first search only performs a search");
  await UrlbarTestUtils.assertSearchMode(window, null);

  let notificationBar =
    window.gNotificationBox.getNotificationWithValue(NOTIFICATION_VALUE);
  Assert.ok(!notificationBar, "No notification is shown");

  let notificationPromise = BrowserTestUtils.waitForGlobalNotificationBar(
    window,
    NOTIFICATION_VALUE
  );
  await loadUri(ENGINE_TEST_URL);
  await performContextualSearch("newsearch");
  let notification = await notificationPromise;

  let promiseEngineAdded = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.ADDED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
  notification.buttonContainer
    .querySelector("[data-l10n-id=install-search-engine-add]")
    .click();
  await promiseEngineAdded;

  Assert.ok(
    (await Services.search.getVisibleEngines()).length > initialEngines.length,
    "New engine was installed"
  );

  await loadUri(ENGINE_TEST_URL);
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "search",
  });
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeKey("KEY_Tab");
  EventUtils.synthesizeKey("KEY_Enter");
  await searchPromise;

  info("The third search uses installed engine and enters search mode");
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: "Foo",
    entry: "keywordoffer",
  });

  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window);

  let engine = Services.search.getEngineByName("Foo");
  await Services.search.removeEngine(engine);
  Services.search.wrappedJSObject._settings.setMetaDataAttribute(
    "contextual-engines-seen",
    {}
  );
});
