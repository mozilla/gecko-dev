/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { CustomizableUITestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/CustomizableUITestUtils.sys.mjs"
);
let gCUITestUtils = new CustomizableUITestUtils(window);

add_setup(async function () {
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    url: "https://example.com/",
    title: "Bookmark 1",
  });

  await PlacesUtils.history.insert({
    url: "https://example.com/2",
    title: "Hist",
    visits: [{ date: new Date() }],
  });

  registerCleanupFunction(async function () {
    await PlacesUtils.bookmarks.eraseEverything();
  });
});

add_task(async function testCloseOnNewtab() {
  let newtabItem = document.getElementById("placesContext_open:newtab");

  let promiseTabOpened = BrowserTestUtils.waitForNewTab(gBrowser, null);
  await testContextmenuClosesPopup(openBookmarkPanel, "Bookmark 1", newtabItem);
  let newTab = await promiseTabOpened;
  BrowserTestUtils.removeTab(newTab);
});

add_task(async function testCloseOnForget() {
  let newtabItem = document.getElementById("placesContext_deleteHost");
  await testContextmenuClosesPopup(openHistoryPanelPanel, "Hist", newtabItem);
  document.getElementById("window-modal-dialog").close();
});

add_task(async function testCloseOnCreateBookmark() {
  let newtabItem = document.getElementById("placesContext_createBookmark");
  await testContextmenuClosesPopup(openHistoryPanelPanel, "Hist", newtabItem);
  document.getElementById("window-modal-dialog").close();
});

add_task(async function testCloseOnNewcontainertab() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.userContext.newTabContainerOnLeftClick.enabled", true],
      ["privacy.userContext.enabled", true],
    ],
  });
  let contextMenuParent = document.getElementById(
    "placesContext_open:newcontainertab"
  );

  let promiseTabOpened = BrowserTestUtils.waitForNewTab(gBrowser, null);
  await testContextmenuClosesPopup(
    openBookmarkPanel,
    "Bookmark 1",
    contextMenuParent,
    true
  );
  let newTab = await promiseTabOpened;
  BrowserTestUtils.removeTab(newTab);
  await SpecialPowers.popPrefEnv();
});

async function openBookmarkPanel() {
  if (window.PanelUI.panel.state == "open") {
    await gCUITestUtils.hideMainMenu();
  }
  await gCUITestUtils.openMainMenu();

  let BMview;
  document.getElementById("appMenu-bookmarks-button").click();
  BMview = document.getElementById("PanelUI-bookmarks");
  let promise = BrowserTestUtils.waitForEvent(BMview, "ViewShown");
  await promise;
  info("Bookmarks panel shown.");

  return document.getElementById("panelMenu_bookmarksMenu");
}

async function openHistoryPanelPanel() {
  if (window.PanelUI.panel.state == "open") {
    await gCUITestUtils.hideMainMenu();
  }
  await gCUITestUtils.openMainMenu();

  let BMview;
  document.getElementById("appMenu-history-button").click();
  BMview = document.getElementById("PanelUI-history");
  let promise = BrowserTestUtils.waitForEvent(BMview, "ViewShown");
  await promise;
  info("History panel shown.");

  return document.getElementById("appMenu_historyMenu");
}

// openFirstChild is useful for selecting open in new container
// because it's in a submenu.
async function testContextmenuClosesPopup(
  panelOpener,
  name,
  contextMenuItem,
  openFirstChild = false
) {
  let list = await panelOpener();

  let menuitem = [...list.children].find(node => node.label == name);
  let cm = document.getElementById("placesContext");

  let shown = BrowserTestUtils.waitForEvent(cm, "popupshown");
  EventUtils.synthesizeMouseAtCenter(menuitem, {
    type: "contextmenu",
    button: 2,
  });
  await shown;

  let hidden = BrowserTestUtils.waitForEvent(cm, "popuphidden");
  if (openFirstChild) {
    let menuPopup = contextMenuItem.menupopup;
    let menuPopupPromise = BrowserTestUtils.waitForEvent(
      menuPopup,
      "popupshown"
    );
    contextMenuItem.openMenu(true);
    await menuPopupPromise;
    cm.activateItem(menuPopup.childNodes[0]);
  } else {
    cm.activateItem(contextMenuItem);
  }
  await hidden;

  Assert.equal(window.PanelUI.panel.state, "closed", "The panel was closed.");
}
