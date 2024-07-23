/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { SearchWidgetTracker } = ChromeUtils.importESModule(
  "resource:///modules/SearchWidgetTracker.sys.mjs"
);

const SEARCH_BAR_LAST_USED_PREF_NAME = "browser.search.widget.lastUsed";

add_task(async function checkSearchBarPresent() {
  await gCUITestUtils.addSearchBar();
  Services.prefs.setStringPref(
    SEARCH_BAR_LAST_USED_PREF_NAME,
    new Date("2022").toISOString()
  );

  Assert.ok(
    BrowserSearch.searchBar,
    "Search bar should be present in the Nav bar"
  );
  SearchWidgetTracker._updateSearchBarVisibilityBasedOnUsage();
  Assert.ok(
    !BrowserSearch.searchBar,
    "Search bar should not be present in the Nav bar"
  );
  Assert.ok(
    !CustomizableUI.getPlacementOfWidget("search-container"),
    "Should remove the search bar"
  );
  Services.prefs.clearUserPref(SEARCH_BAR_LAST_USED_PREF_NAME);
  gCUITestUtils.removeSearchBar();
});
