/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PlacesTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PlacesTestUtils.sys.mjs"
);

add_task(async function test_BOOKMARK_CURRENT_TAB_WITHOUT_DIALOG() {
  const action = {
    type: "BOOKMARK_CURRENT_TAB",
    data: { shouldHideDialog: true },
  };
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/"
  );

  let bookmarkPromise = PlacesTestUtils.waitForNotification(
    "bookmark-added",
    () => "https://example.com/"
  );

  await SMATestUtils.validateAction(action);
  await SpecialMessageActions.handleAction(action, gBrowser);
  await bookmarkPromise;

  let bookmark = await PlacesUtils.bookmarks.fetch({
    url: "https://example.com/",
  });

  Assert.ok(bookmark, "Found expected bookmark");

  BrowserTestUtils.removeTab(tab);
  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function test_BOOKMARK_CURRENT_TAB_WITH_DIALOG() {
  const action = {
    type: "BOOKMARK_CURRENT_TAB",
  };
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/"
  );

  let dialogPromise = BrowserTestUtils.promiseAlertDialogOpen(
    null,
    "chrome://browser/content/places/bookmarkProperties.xhtml",
    {
      isSubDialog: true,
    }
  );

  let bookmarkPromise = PlacesTestUtils.waitForNotification(
    "bookmark-added",
    () => "https://example.com/"
  );

  // Validating action and firing it
  await SMATestUtils.validateAction(action);
  await SpecialMessageActions.handleAction(action, gBrowser);

  // Waiting for bookmark dialog to open
  let dialogWindow = await dialogPromise;

  // Waiting for focus and sub dialog to be ready
  await SimpleTest.promiseFocus(dialogWindow);
  await dialogWindow.document.mozSubdialogReady;

  const dialogClosed = BrowserTestUtils.waitForEvent(
    window,
    "DOMModalDialogClosed"
  );

  // Pressing "save" button in the bookmark dialog
  dialogWindow.document
    .getElementById("bookmarkpropertiesdialog")
    .getButton("accept")
    .click();

  await dialogClosed;
  await bookmarkPromise;

  let bookmark = await PlacesUtils.bookmarks.fetch({
    url: "https://example.com/",
  });

  Assert.ok(bookmark, "Found expected bookmark");

  BrowserTestUtils.removeTab(tab);
  await PlacesUtils.bookmarks.eraseEverything();
});
