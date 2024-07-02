/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let win;

add_setup(async () => {
  win = await BrowserTestUtils.openNewBrowserWindow();
  registerCleanupFunction(() => BrowserTestUtils.closeWindow(win));
});

add_task(async function macDockWindowOpenInheritsSidebar() {
  const bookmarksSidebar = "viewBookmarksSidebar";
  // Open bookmarks sidebar:
  const { SidebarController } = win;
  await SidebarController.show(bookmarksSidebar);
  Assert.equal(
    SidebarController.currentID,
    bookmarksSidebar,
    "Bookmarks sidebar should be shown in original window."
  );
  Assert.ok(
    SidebarController.isOpen,
    "Sidebar should be open in original window."
  );

  // Open another window via the dock menu entry.
  let { hiddenDOMWindow } = Services.appShell;

  let nextWindowPromise = BrowserTestUtils.waitForNewWindow();

  let menuItem = hiddenDOMWindow.document.getElementById(
    "macDockMenuNewWindow"
  );
  menuItem.doCommand();
  let nextWindow = await nextWindowPromise;

  // Check the next window also gets a sidebar:
  Assert.equal(
    SidebarController.currentID,
    bookmarksSidebar,
    "Bookmarks sidebar should be shown in new window."
  );
  Assert.ok(SidebarController.isOpen, "Sidebar should be open in new window.");

  // Ensure we close the next window.
  await BrowserTestUtils.closeWindow(nextWindow);
});
