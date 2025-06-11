/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["sidebar.revamp", true],
      ["sidebar.verticalTabs", true],
    ],
  });
});

registerCleanupFunction(async () => {
  await SpecialPowers.popPrefEnv();
  gBrowser.removeAllTabsBut(gBrowser.tabs[0]);
});

add_task(async function test_button_removed() {
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

  const tabstrip = document.getElementById("tabbrowser-tabs");
  info("Tab orientation should change to horizontal.");
  await BrowserTestUtils.waitForMutationCondition(
    tabstrip,
    { attributeFilter: ["orient"] },
    () => tabstrip.getAttribute("orient") === "horizontal"
  );

  CustomizableUI.reset();
  CustomizableUI.addWidgetToArea("sidebar-button", "nav-bar", 0);
});

add_task(async function test_button_moved() {
  const { SidebarController } = window;
  await SidebarController.show("viewBookmarksSidebar");

  let sidebarButton = document.getElementById("sidebar-button");
  let sidebarMain = document.getElementById("sidebar-main");
  let sidebarBox = document.getElementById("sidebar-box");

  await startCustomizing();
  is(gBrowser.tabs.length, 2, "Should have 2 tabs");
  let nonCustomizingTab = gBrowser.tabContainer.querySelector(
    "tab:not([customizemode=true])"
  );
  let finishedCustomizing = BrowserTestUtils.waitForEvent(
    gNavToolbox,
    "aftercustomization"
  );
  // Add the button to the tabstrip.
  CustomizableUI.addWidgetToArea("sidebar-button", "TabsToolbar");
  // This is a little tricky to test "properly": the button removal
  // handling code inside sidebar is async, but we are asserting that
  // it does nothing, so there's nothing to wait for.
  // In practice, switching tabs and customizing are both sufficiently
  // slow that the test would fail if the code did decide to hide the
  // sidebar again.
  await BrowserTestUtils.switchTab(gBrowser, nonCustomizingTab);
  await finishedCustomizing;

  ok(sidebarButton, "Sidebar button has not been removed.");
  ok(!sidebarMain.hidden, "Sidebar launcher has not been hidden.");
  ok(!sidebarBox.hidden, "Sidebar panel has not been hidden.");

  CustomizableUI.reset();
  CustomizableUI.addWidgetToArea("sidebar-button", "nav-bar", 0);
});
