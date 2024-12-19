/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the Unified Search Button with an empty search input.
 */

"use strict";

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.scotchBonnet.enableOverride", true]],
  });
});

add_task(
  async function test_search_mode_chiclet_on_content_area_click_home_page() {
    let searchModeSwitcherButton = window.document.getElementById(
      "urlbar-searchmode-switcher"
    );

    await BrowserTestUtils.waitForCondition(
      () => BrowserTestUtils.isVisible(searchModeSwitcherButton),
      "Wait until unified search button is visible"
    );

    info("Open search mode switcher");
    let popup = await UrlbarTestUtils.openSearchModeSwitcher(window);
    let bookmarksSearchModeButton = popup.querySelector(
      "#search-button-bookmarks"
    );

    let popupHidden = BrowserTestUtils.waitForEvent(popup, "popuphidden");
    bookmarksSearchModeButton.click();
    await popupHidden;

    await UrlbarTestUtils.assertSearchMode(window, {
      source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
      entry: "searchbutton",
    });

    EventUtils.synthesizeMouseAtCenter(document.body, {}, window);

    await UrlbarTestUtils.assertSearchMode(window, {
      source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
      entry: "searchbutton",
    });
  }
);

add_task(
  async function test_search_mode_chiclet_on_content_area_click_loaded_site() {
    // test that search mode chiclet remains on loaded sites as well
    let newTab = await BrowserTestUtils.openNewForegroundTab(
      window.gBrowser,
      "https://example.com"
    );

    // Make unified search button appear
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "",
    });

    let searchModeSwitcherButton = window.document.getElementById(
      "urlbar-searchmode-switcher"
    );

    await BrowserTestUtils.waitForCondition(
      () => BrowserTestUtils.isVisible(searchModeSwitcherButton),
      "Wait until unified search button is visible"
    );

    info("Open search mode switcher");
    let popup = await UrlbarTestUtils.openSearchModeSwitcher(window);
    let bookmarksSearchModeButton = popup.querySelector(
      "#search-button-bookmarks"
    );

    let popupHidden = BrowserTestUtils.waitForEvent(popup, "popuphidden");
    bookmarksSearchModeButton.click();
    await popupHidden;

    await UrlbarTestUtils.assertSearchMode(window, {
      source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
      entry: "searchbutton",
    });

    EventUtils.synthesizeMouseAtCenter(document.body, {}, window);

    await UrlbarTestUtils.assertSearchMode(window, {
      source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
      entry: "searchbutton",
    });

    BrowserTestUtils.removeTab(newTab);
  }
);
