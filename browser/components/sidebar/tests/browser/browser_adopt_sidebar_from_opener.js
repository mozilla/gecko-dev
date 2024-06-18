/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
});

add_setup(() => SpecialPowers.pushPrefEnv({ set: [["sidebar.revamp", true]] }));

add_task(async function test_adopt_from_window() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const { document } = win;
  const sidebar = document.querySelector("sidebar-main");
  ok(sidebar, "Sidebar is shown.");
  await sidebar.updateComplete;
  await toggleSidebarPanel(win, "viewCustomizeSidebar");

  // Set width
  let sidebarBox = document.getElementById("sidebar-box");
  await BrowserTestUtils.waitForCondition(
    () => BrowserTestUtils.isVisible(sidebarBox),
    "Sidebar box is visible"
  );
  sidebarBox.style.width = "100px";

  // Open a new window from the window containing the open sidebar
  const newWin = lazy.BrowserWindowTracker.openWindow({
    openerWindow: win,
  });

  // Check category of new window sidebar is that of opener window sidebar
  let newSidebarBox = document.getElementById("sidebar-box");
  await BrowserTestUtils.waitForCondition(
    () => BrowserTestUtils.isVisible(newSidebarBox),
    "New sidebar box is visible"
  );
  await BrowserTestUtils.waitForCondition(
    () => !!newSidebarBox.getAttribute("sidebarcommand"),
    "Category has been set"
  );
  is(
    newSidebarBox.getAttribute("sidebarcommand"),
    "viewCustomizeSidebar",
    "Customize side panel is open, as adopted from opener window sidebar"
  );

  // Check width of new window sidebar is that of opener window sidebar
  await BrowserTestUtils.waitForCondition(
    () => !!newSidebarBox.style.width,
    "Width has been set"
  );
  is(
    newSidebarBox.style.width,
    "100px",
    "New window sidebar width is the same as opener window sidebar width"
  );

  await BrowserTestUtils.closeWindow(newWin);
  await BrowserTestUtils.closeWindow(win);
});
