/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [["sidebar.revamp", true]],
  });
});

registerCleanupFunction(async () => {
  await SpecialPowers.popPrefEnv();
  gBrowser.removeAllTabsBut(gBrowser.tabs[0]);
});

add_task(async function () {
  const { SidebarController } = window;
  await SidebarController.show("viewBookmarksSidebar");

  let sidebarButton = await BrowserTestUtils.waitForCondition(
    () => document.getElementById("sidebar-button"),
    "Sidebar button is shown."
  );
  ok(sidebarButton, "Sidebar button is shown.");
  let sidebarMain = document.getElementById("sidebar-main");
  ok(sidebarMain, "Sidebar launcher is shown.");
  let sidebarBox = await BrowserTestUtils.waitForCondition(
    () => document.getElementById("sidebar-box"),
    "Sidebar panel is shown."
  );
  ok(sidebarBox, "Sidebar panel is shown.");

  await startCustomizing();
  is(gBrowser.tabs.length, 2, "Should have 2 tabs");
  let nonCustomizingTab = gBrowser.tabContainer.querySelector(
    "tab:not([customizemode=true])"
  );
  let finishedCustomizing = BrowserTestUtils.waitForEvent(
    gNavToolbox,
    "aftercustomization"
  );
  CustomizableUI.removeWidgetFromArea("sidebar-button");
  await BrowserTestUtils.switchTab(gBrowser, nonCustomizingTab);
  await finishedCustomizing;
  await BrowserTestUtils.waitForCondition(() => {
    sidebarButton = document.getElementById("sidebar-button");
    return !sidebarButton && sidebarMain.hidden && sidebarBox.hidden;
  }, "Sidebar button, panel and launcher are not present");

  ok(!sidebarButton, "Sidebar button has been removed.");
  ok(sidebarMain.hidden, "Sidebar launcher has been hidden.");
  ok(sidebarBox.hidden, "Sidebar panel has been hidden.");
});
