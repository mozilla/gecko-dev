/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_ENGINE_NAME = "Foo";
const TEST_ENGINE_BASENAME = "testEngine.xml";
const SEARCH_WORD = "abc";

let searchBar;
let searchPopup;
let searchIcon;

add_setup(async function () {
  searchBar = await gCUITestUtils.addSearchBar();
  registerCleanupFunction(() => {
    gCUITestUtils.removeSearchBar();
  });

  searchPopup = document.getElementById("PopupSearchAutoComplete");
  searchIcon = searchBar.querySelector(".searchbar-search-button");

  await SearchTestUtils.installOpenSearchEngine({
    url: getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME,
    setAsDefault: true,
  });
});

add_task(async function nonEmptySearch() {
  searchBar.value = SEARCH_WORD;

  let promise = promiseEvent(searchPopup, "popupshown");
  info("Opening search panel");
  EventUtils.synthesizeMouseAtCenter(searchIcon, {});
  await promise;

  let engineNameBox = searchPopup.querySelector(".searchbar-engine-name");

  promise = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    `http://mochi.test:8888/browser/browser/components/search/test/browser/?search&test=${SEARCH_WORD}`
  );
  EventUtils.synthesizeMouseAtCenter(engineNameBox, {});
  await promise;
  info("Search successful");
});

add_task(async function emptySearch() {
  searchBar.value = "";

  let promise = promiseEvent(searchPopup, "popupshown");
  info("Opening search panel");
  EventUtils.synthesizeMouseAtCenter(searchIcon, {});
  await promise;

  let engineNameBox = searchPopup.querySelector(".searchbar-engine-name");

  EventUtils.synthesizeMouseAtCenter(engineNameBox, {});

  await TestUtils.waitForTick();
  Assert.equal(
    gBrowser.selectedBrowser.ownerDocument.activeElement,
    searchBar.textbox,
    "Focus stays in the searchbar"
  );
});
