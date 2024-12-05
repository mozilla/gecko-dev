/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test to ensure that on "mousedown" in Toolbar we set Speculative Connection
 */

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
const { CustomizableUITestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/CustomizableUITestUtils.sys.mjs"
);

let gCUITestUtils = new CustomizableUITestUtils(window);
let spy = sinon
  .stub(PlacesUIUtils, "setupSpeculativeConnection")
  .returns(Promise.resolve());

add_setup(async function () {
  await PlacesUtils.bookmarks.eraseEverything();

  let toolbar = document.getElementById("PersonalToolbar");
  ok(toolbar, "PersonalToolbar should not be null");

  if (toolbar.collapsed) {
    await promiseSetToolbarVisibility(toolbar, true);
    registerCleanupFunction(function () {
      return promiseSetToolbarVisibility(toolbar, false);
    });
  }
  registerCleanupFunction(async () => {
    await PlacesUtils.bookmarks.eraseEverything();
    spy.restore();
  });
});

add_task(async function checkToolbarSpeculativeConnection() {
  let toolbarBookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: "https://example.com/",
    title: "Bookmark 1",
  });
  let toolbarNode = getToolbarNodeForItemGuid(toolbarBookmark.guid);

  info("Synthesize right click on bookmark");
  mouseDownUp(toolbarNode, 2);
  Assert.ok(spy.notCalled, "Speculative connection for Toolbar not called");

  info("Synthesize left click on bookmark");
  mouseDownUp(toolbarNode, 0);
  Assert.ok(spy.calledOnce, "Speculative connection for Toolbar called");

  info("Synthesize middle click on bookmark");
  let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);
  mouseDownUp(toolbarNode, 1);
  Assert.ok(spy.calledTwice, "Speculative connection for Toolbar called");

  let tab = await newTabPromise;
  BrowserTestUtils.removeTab(tab);

  spy.resetHistory();
});

add_task(async function checkPanelviewSpeculativeConnection() {
  await PlacesUtils.bookmarks.eraseEverything();

  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    url: "https://example.com/",
    title: "Bookmark 3",
  });

  let menuNode = await openBookmarksPanelGetBookmark3();
  info("Synthesize right click on bookmark");
  mouseDownUp(menuNode, 2);
  Assert.ok(
    spy.notCalled,
    "Speculative connection for Panel Button not called"
  );

  let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);
  info("Synthesize middle click on bookmark");
  mouseDownUp(menuNode, 1);
  Assert.ok(spy.calledOnce, "Speculative connection for Panel Button called");
  let tab = await newTabPromise;
  BrowserTestUtils.removeTab(tab);

  menuNode = await openBookmarksPanelGetBookmark3();
  info("Synthesize left click on bookmark");
  mouseDownUp(menuNode, 0);
  Assert.ok(spy.calledTwice, "Speculative connection for Panel Button called");

  spy.resetHistory();
});

add_task(async function checkMenuSpeculativeConnection() {
  await PlacesUtils.bookmarks.eraseEverything();

  info("Placing a Menu widget");
  let origBMBlocation = CustomizableUI.getPlacementOfWidget(
    "bookmarks-menu-button"
  );
  // Ensure BMB is available in UI.
  if (!origBMBlocation) {
    CustomizableUI.addWidgetToArea(
      "bookmarks-menu-button",
      CustomizableUI.AREA_NAVBAR
    );
  }

  registerCleanupFunction(async function () {
    await PlacesUtils.bookmarks.eraseEverything();
    // if BMB was not originally in UI, remove it.
    if (!origBMBlocation) {
      CustomizableUI.removeWidgetFromArea("bookmarks-menu-button");
    }
  });

  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    url: "https://example.com/",
    title: "Bookmark 2",
  });

  let menuNode = await openBookmarksMenuGetBookmark2();
  info("Synthesize right click on bookmark");
  mouseDownUp(menuNode, 2);
  Assert.ok(spy.notCalled, "Speculative connection for Menu Button not called");

  let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);
  info("Synthesize middle click on bookmark");
  mouseDownUp(menuNode, 1);
  Assert.ok(spy.calledOnce, "Speculative connection for Menu Button called");
  let tab = await newTabPromise;
  BrowserTestUtils.removeTab(tab);

  menuNode = await openBookmarksMenuGetBookmark2();
  info("Synthesize left click on bookmark");
  mouseDownUp(menuNode, 0);
  Assert.ok(spy.calledTwice, "Speculative connection for Menu Button called");

  spy.resetHistory();
});

function mouseDownUp(node, button) {
  EventUtils.synthesizeMouseAtCenter(node, {
    type: "mousedown",
    button,
  });
  EventUtils.synthesizeMouseAtCenter(node, {
    type: "mouseup",
    button,
  });
}

async function openBookmarksMenuGetBookmark2() {
  let BMB = document.getElementById("bookmarks-menu-button");
  let BMBpopup = document.getElementById("BMB_bookmarksPopup");
  let promiseEvent = BrowserTestUtils.waitForEvent(BMBpopup, "popupshown");
  EventUtils.synthesizeMouseAtCenter(BMB, {});
  await promiseEvent;
  info("Popupshown on Bookmarks-Menu-Button");

  return [...BMBpopup.children].find(node => node.label == "Bookmark 2");
}

async function openBookmarksPanelGetBookmark3() {
  await gCUITestUtils.openMainMenu();
  let BMview;
  document.getElementById("appMenu-bookmarks-button").click();
  BMview = document.getElementById("PanelUI-bookmarks");
  let promise = BrowserTestUtils.waitForEvent(BMview, "ViewShown");
  await promise;
  info("Bookmarks panel shown.");

  let list = document.getElementById("panelMenu_bookmarksMenu");
  return [...list.children].find(node => node.label == "Bookmark 3");
}
