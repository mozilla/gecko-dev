"use strict";

const TEST_ENGINE_NAME = "Foo";
const TEST_ENGINE_BASENAME = "testEngine.xml";

let searchbar;
let searchIcon;
let searchPopup;
let oneOffInstance;
let oneOffButtons;

add_setup(async function () {
  searchbar = await gCUITestUtils.addSearchBar();
  registerCleanupFunction(() => {
    gCUITestUtils.removeSearchBar();
  });
  searchIcon = searchbar.querySelector(".searchbar-search-button");
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

add_task(async function testNewtabEmpty() {
  await openPopup("abc");
  let oneOffButton = findOneOff(TEST_ENGINE_NAME);

  let promise = BrowserTestUtils.waitForNewTab(gBrowser);
  await activateContextMenuItem(
    oneOffButton,
    ".search-one-offs-context-open-in-new-tab"
  );
  let tab = await promise;

  // By default the search will open in the background and the popup will stay open
  await closePopup();

  Assert.equal(
    tab.linkedBrowser.currentURI.spec,
    "http://mochi.test:8888/browser/browser/components/search/test/browser/?search&test=abc",
    "Expected search tab should have loaded"
  );

  BrowserTestUtils.removeTab(tab);
});

add_task(async function testNewtabNonempty() {
  await openPopup("");
  let oneOffButton = findOneOff(TEST_ENGINE_NAME);

  let promise = BrowserTestUtils.waitForNewTab(gBrowser);
  await activateContextMenuItem(
    oneOffButton,
    ".search-one-offs-context-open-in-new-tab"
  );
  let tab = await promise;

  // By default the search form will open in the background and the popup will stay open
  await closePopup();

  Assert.equal(
    tab.linkedBrowser.currentURI.spec,
    "http://mochi.test:8888/browser/browser/components/search/test/browser/",
    "Search form should have loaded in new tab"
  );

  BrowserTestUtils.removeTab(tab);
});

function findOneOff(engineName) {
  let oneOffChildren = [...oneOffButtons.children];
  let oneOffButton = oneOffChildren.find(
    node => node.engine?.name == engineName
  );
  Assert.notEqual(
    oneOffButton,
    undefined,
    `One-off for ${engineName} should exist`
  );
  return oneOffButton;
}

async function activateContextMenuItem(oneOffButton, itemID) {
  let contextMenu = oneOffInstance.querySelector(
    ".search-one-offs-context-menu"
  );
  let promise = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(oneOffButton, {
    type: "contextmenu",
    button: 2,
  });
  await promise;

  let menuItem = contextMenu.querySelector(itemID);
  contextMenu.activateItem(menuItem);
}

async function openPopup(searchBarValue) {
  searchbar.focus();
  searchbar.value = searchBarValue;
  if (searchbar.textbox.popupOpen) {
    info("searchPanel is already open");
    return;
  }
  let shownPromise = promiseEvent(searchPopup, "popupshown");
  let builtPromise = promiseEvent(oneOffInstance, "rebuild");
  info("Opening search panel");
  EventUtils.synthesizeMouseAtCenter(searchIcon, {});
  await Promise.all([shownPromise, builtPromise]);
}

async function closePopup() {
  let promise = promiseEvent(searchPopup, "popuphidden");
  info("Closing search panel");
  EventUtils.synthesizeKey("KEY_Escape");
  await promise;
}
