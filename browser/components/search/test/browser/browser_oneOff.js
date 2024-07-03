/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_ENGINE_NAME = "Foo";
const TEST_ENGINE_BASENAME = "testEngine.xml";
const SEARCH_WORD = "abc";

let searchBar;
let searchIcon;
let oneOffButtons;
let searchPopup;
let oneOffInstance;

add_setup(async function () {
  searchBar = await gCUITestUtils.addSearchBar();
  registerCleanupFunction(() => {
    gCUITestUtils.removeSearchBar();
  });
  searchIcon = searchBar.querySelector(".searchbar-search-button");

  searchPopup = document.getElementById("PopupSearchAutoComplete");
  oneOffInstance = searchPopup.oneOffButtons;
  oneOffButtons = oneOffInstance.buttons;

  // Set default engine so no external requests are made.
  await SearchTestUtils.installSearchExtension(
    {
      name: "MozSearch",
      keyword: "mozalias",
    },
    { setAsDefault: true }
  );
  // Add the engine that will be used.
  await SearchTestUtils.installOpenSearchEngine({
    url: getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME,
  });
});

add_task(async function nonEmptySearch() {
  searchBar.value = SEARCH_WORD;

  let shownPromise = promiseEvent(searchPopup, "popupshown");
  let builtPromise = promiseEvent(oneOffInstance, "rebuild");
  info("Opening search panel");
  EventUtils.synthesizeMouseAtCenter(searchIcon, {});
  await Promise.all([shownPromise, builtPromise]);

  // Get the one-off button for the test engine.
  let oneOffButton;
  for (let node of oneOffButtons.children) {
    if (node.engine && node.engine.name == TEST_ENGINE_NAME) {
      oneOffButton = node;
      break;
    }
  }
  Assert.notEqual(
    oneOffButton,
    undefined,
    "One-off for test engine should exist"
  );

  let promise = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    `http://mochi.test:8888/browser/browser/components/search/test/browser/?search&test=${SEARCH_WORD}`
  );
  EventUtils.synthesizeMouseAtCenter(oneOffButton, {});
  await promise;
  info("Search successful");
});

add_task(async function emptySearch() {
  searchBar.value = "";

  let shownPromise = promiseEvent(searchPopup, "popupshown");
  let builtPromise = promiseEvent(oneOffInstance, "rebuild");
  info("Opening search panel");
  EventUtils.synthesizeMouseAtCenter(searchIcon, {});
  await Promise.all([shownPromise, builtPromise]);

  // Get the one-off button for the test engine.
  let oneOffButton;
  for (let node of oneOffButtons.children) {
    if (node.engine && node.engine.name == TEST_ENGINE_NAME) {
      oneOffButton = node;
      break;
    }
  }
  Assert.notEqual(
    oneOffButton,
    undefined,
    "One-off for test engine should exist"
  );

  EventUtils.synthesizeMouseAtCenter(oneOffButton, {});

  await TestUtils.waitForTick();
  Assert.equal(
    gBrowser.selectedBrowser.ownerDocument.activeElement,
    searchBar.textbox,
    "Focus stays in the searchbar"
  );
});
