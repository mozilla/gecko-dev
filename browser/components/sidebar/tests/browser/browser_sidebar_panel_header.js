/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(() => SpecialPowers.pushPrefEnv({ set: [["sidebar.revamp", true]] }));

add_task(async function test_close_panel() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const { document } = win;
  const sidebar = document.querySelector("sidebar-main");
  ok(sidebar, "Sidebar is shown.");
  await sidebar.updateComplete;
  await toggleSidebarPanel(win, "viewCustomizeSidebar");
  let customizeDocument = win.SidebarController.browser.contentDocument;
  const customizeComponent =
    customizeDocument.querySelector("sidebar-customize");
  const sidebarPanelHeader = customizeComponent.shadowRoot.querySelector(
    "sidebar-panel-header"
  );
  let closeButton = sidebarPanelHeader.closeButton;
  closeButton.click();
  ok(
    !document.querySelector("sidebar-customize"),
    "Sidebar customize panel has been closed."
  );

  await BrowserTestUtils.closeWindow(win);
});
