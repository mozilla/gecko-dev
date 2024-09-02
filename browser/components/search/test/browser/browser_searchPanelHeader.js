/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_ENGINE_NAME = "Foo";
const TEST_ENGINE_BASENAME = "testEngine.xml";
const SEARCH_WORD = "abc";

let searchBar;
let searchPopup;
let searchIcon;
let win;

add_setup(async function () {
  await gCUITestUtils.addSearchBar();
  win = await BrowserTestUtils.openNewBrowserWindow();

  searchBar = win.BrowserSearch.searchBar;
  searchPopup = win.document.getElementById("PopupSearchAutoComplete");
  searchIcon = searchBar.querySelector(".searchbar-search-button");

  registerCleanupFunction(async () => {
    await BrowserTestUtils.closeWindow(win);
    // This is necessary to prevent leaking the window.
    // TODO: Why does this lead to a leak?
    searchBar = undefined;
    searchPopup = undefined;
    searchIcon = undefined;
    win = undefined;
    gCUITestUtils.removeSearchBar();
  });
  await SearchTestUtils.installOpenSearchEngine({
    url: getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME,
    setAsDefault: true,
  });
});

add_task(async function nonEmptySearch() {
  await openPopup(SEARCH_WORD);

  let engineNameBox = searchPopup.querySelector(".searchbar-engine-name");

  let promise = BrowserTestUtils.browserLoaded(
    win.gBrowser.selectedBrowser,
    false,
    `http://mochi.test:8888/browser/browser/components/search/test/browser/?search&test=${SEARCH_WORD}`
  );
  EventUtils.synthesizeMouseAtCenter(engineNameBox, {}, win);
  await promise;
  info("Search successful");
});

add_task(async function emptySearch() {
  await openPopup("");

  let engineNameBox = searchPopup.querySelector(".searchbar-engine-name");
  EventUtils.synthesizeMouseAtCenter(engineNameBox, {}, win);
  await TestUtils.waitForTick();

  Assert.equal(
    win.gBrowser.selectedBrowser.ownerDocument.activeElement,
    searchBar.textbox,
    "Focus stays in the searchbar"
  );
});

add_task(async function emptySearchShift() {
  await openPopup("");

  let engineNameBox = searchPopup.querySelector(".searchbar-engine-name");

  let promise = BrowserTestUtils.browserLoaded(
    win.gBrowser.selectedBrowser,
    false,
    "http://mochi.test:8888/"
  );
  EventUtils.synthesizeMouseAtCenter(engineNameBox, { shiftKey: true }, win);
  await promise;
  info("Opening search form successful");
});

async function openPopup(searchBarValue) {
  searchBar.focus();
  searchBar.value = searchBarValue;
  if (searchBar.textbox.popupOpen) {
    info("searchPanel is already open");
    return;
  }
  let shownPromise = promiseEvent(searchPopup, "popupshown");
  let builtPromise = promiseEvent(searchPopup.oneOffButtons, "rebuild");
  info("Opening search panel");
  EventUtils.synthesizeMouseAtCenter(searchIcon, {}, win);
  await Promise.all([shownPromise, builtPromise]);
}
