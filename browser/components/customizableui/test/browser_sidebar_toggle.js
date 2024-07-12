/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

registerCleanupFunction(async function () {
  await resetCustomization();

  // Ensure sidebar is hidden after each test:
  if (!document.getElementById("sidebar-box").hidden) {
    SidebarController.hide();
  }
});

var showSidebar = async function (win = window) {
  let button = win.document.getElementById("sidebar-button");
  let sidebarFocusedPromise = BrowserTestUtils.waitForEvent(
    win.document,
    "SidebarFocused"
  );
  EventUtils.synthesizeMouseAtCenter(button, {}, win);
  await sidebarFocusedPromise;
  ok(win.SidebarController.isOpen, "Sidebar is opened");
  ok(button.hasAttribute("checked"), "Toolbar button is checked");
};

var hideSidebar = async function (win = window) {
  let button = win.document.getElementById("sidebar-button");
  EventUtils.synthesizeMouseAtCenter(button, {}, win);
  ok(!win.SidebarController.isOpen, "Sidebar is closed");
  ok(!button.hasAttribute("checked"), "Toolbar button isn't checked");
};

// Check the sidebar widget shows the default items
add_task(async function () {
  let sidebarRevampEnabled = Services.prefs.getBoolPref(
    "sidebar.revamp",
    false
  );
  if (!sidebarRevampEnabled) {
    CustomizableUI.addWidgetToArea("sidebar-button", "nav-bar");

    await showSidebar();
    is(
      SidebarController.currentID,
      "viewBookmarksSidebar",
      "Default sidebar selected"
    );
    await SidebarController.show("viewHistorySidebar");

    await hideSidebar();
    await showSidebar();
    is(
      SidebarController.currentID,
      "viewHistorySidebar",
      "Selected sidebar remembered"
    );

    await hideSidebar();
  } else {
    const sidebar = document.querySelector("sidebar-main");
    ok(sidebar, "Sidebar is shown.");
    for (const [index, toolButton] of sidebar.toolButtons.entries()) {
      await SidebarController.show(toolButton.getAttribute("view"));
      is(
        SidebarController.currentID,
        toolButton.getAttribute("view"),
        `${toolButton.getAttribute("view")} sidebar selected`
      );
      if (index < sidebar.toolButtons.length - 1) {
        SidebarController.toggle(toolButton.getAttribute("view"));
      }
    }
  }
  let otherWin = await BrowserTestUtils.openNewBrowserWindow();
  if (!sidebarRevampEnabled) {
    await showSidebar(otherWin);
    is(
      otherWin.SidebarController.currentID,
      "viewHistorySidebar",
      "Selected sidebar remembered across windows"
    );
    await hideSidebar(otherWin);
  } else {
    let otherSidebar = otherWin.document.querySelector("sidebar-main");
    let lastTool =
      otherSidebar.toolButtons[otherSidebar.toolButtons.length - 1];
    is(
      otherWin.SidebarController.currentID,
      lastTool.getAttribute("view"),
      "Selected sidebar remembered across windows"
    );
    otherWin.SidebarController.toggle(lastTool.getAttribute("view"));
  }

  await BrowserTestUtils.closeWindow(otherWin);
});
