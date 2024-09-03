/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.scotchBonnet.enableOverride", true]],
  });

  await SearchTestUtils.installSearchExtension({
    name: "Contextual",
    search_url: "https://example.com/browser",
  });
});

add_task(async function test_persist_searchmode() {
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

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "search",
  });

  info("Focus and select the contextual search result");
  let onLoad = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  EventUtils.synthesizeKey("KEY_Tab");
  EventUtils.synthesizeKey("KEY_Enter");
  await onLoad;

  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: "Contextual",
    entry: "other",
  });

  info("Performing another search in the urlbar will remain in searchMode");
  onLoad = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "new search",
  });
  EventUtils.synthesizeKey("KEY_Enter");
  await onLoad;
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: "Contextual",
    entry: "other",
  });

  info("If the user navigates to another host, we exit searchMode");
  onLoad = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    "https://example.org"
  );
  await onLoad;
  await UrlbarTestUtils.assertSearchMode(window, null);
});
