/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.prefs.clearUserPref("sidebar.main.tools");
registerCleanupFunction(() =>
  Services.prefs.clearUserPref("sidebar.main.tools")
);

add_task(async function test_tools_prefs() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const { document } = win;
  const sidebar = document.querySelector("sidebar-main");
  ok(sidebar, "Sidebar is shown.");
  await sidebar.updateComplete;

  is(
    Services.prefs.getStringPref("sidebar.main.tools"),
    "aichat,syncedtabs,history",
    "Default tools pref unchanged"
  );

  // Open customize sidebar
  await toggleSidebarPanel(win, "viewCustomizeSidebar");

  // Set tools
  let customizeDocument = win.SidebarController.browser.contentDocument;
  let customizeComponent = customizeDocument.querySelector("sidebar-customize");
  let toolEntrypointsCount = sidebar.toolButtons.length;
  let checkedInputs = Array.from(customizeComponent.toolInputs).filter(
    input => input.checked
  );
  is(
    checkedInputs.length,
    toolEntrypointsCount,
    `${toolEntrypointsCount} inputs to toggle Firefox Tools are shown in the Customize Menu.`
  );
  let bookmarksInput = Array.from(customizeComponent.toolInputs).find(
    input => input.name === "viewBookmarksSidebar"
  );
  ok(
    !bookmarksInput.checked,
    "The bookmarks input is unchecked initially as Bookmarks are disabled initially."
  );
  for (const toolInput of customizeComponent.toolInputs) {
    let toolDisabledInitialState = !toolInput.checked;
    toolInput.click();
    await BrowserTestUtils.waitForCondition(
      () => {
        let toggledTool = win.SidebarController.toolsAndExtensions.get(
          toolInput.name
        );
        return toggledTool.disabled === !toolDisabledInitialState;
      },
      `The entrypoint for ${toolInput.name} has been ${toolDisabledInitialState ? "enabled" : "disabled"} in the sidebar.`
    );
    toolEntrypointsCount = sidebar.toolButtons.length;
    checkedInputs = Array.from(customizeComponent.toolInputs).filter(
      input => input.checked
    );
    is(
      toolEntrypointsCount,
      checkedInputs.length,
      `The button for the ${toolInput.name} entrypoint has been ${
        toolDisabledInitialState ? "added" : "removed"
      }.`
    );
  }

  const updatedTools = Services.prefs.getStringPref("sidebar.main.tools");
  is(
    updatedTools,
    "aichat,bookmarks",
    "History and syncedtabs have been removed from the pref, and bookmarks added"
  );

  await BrowserTestUtils.closeWindow(win);

  //   Open a new window to check that it uses the pref
  const newWin = await BrowserTestUtils.openNewBrowserWindow();
  const newSidebar = newWin.document.querySelector("sidebar-main");
  ok(newSidebar, "New Window sidebar is shown.");
  await BrowserTestUtils.waitForCondition(
    async () => (await newSidebar.updateComplete) && newSidebar.customizeButton,
    `The sidebar-main component has fully rendered, and the customize button is present.`
  );

  // TO DO: opening the customize category can be removed once bug 1898613 is resolved.
  // Open customize sidebar
  await toggleSidebarPanel(newWin, "viewCustomizeSidebar");

  let newCustomizeDocument = newWin.SidebarController.browser.contentDocument;
  let newCustomizeComponent =
    newCustomizeDocument.querySelector("sidebar-customize");
  let newToolEntrypointsCount = newSidebar.toolButtons.length;
  let newCheckedInputs = Array.from(newCustomizeComponent.toolInputs).filter(
    input => input.checked
  );
  is(
    newCheckedInputs.length,
    newToolEntrypointsCount,
    `${newToolEntrypointsCount} Firefox Tool button is shown in the sidebar matching ${newCheckedInputs.length} shown in the Customize Menu.`
  );
  is(
    newCheckedInputs.length,
    checkedInputs.length,
    "The number of tool inputs checked matches that of the other window's sidebar"
  );
  let newBookmarksInput = Array.from(newCustomizeComponent.toolInputs).find(
    input => input.name === "viewBookmarksSidebar"
  );
  is(
    newBookmarksInput.checked,
    bookmarksInput.checked,
    "The bookmarks input is checked, as suggested by the pref."
  );

  is(
    Services.prefs.getStringPref("sidebar.main.tools"),
    updatedTools,
    "Previously set tools should still be the same"
  );

  await BrowserTestUtils.closeWindow(newWin);
});

/**
 * Check that conditional sidebar tools are added and removed on pref change
 */
add_task(async function test_conditional_tools() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const sidebar = win.document.querySelector("sidebar-main");
  await sidebar.updateComplete;

  const origCount = sidebar.toolButtons.length;
  is(origCount, 1, "Expected number of initial tools");

  await SpecialPowers.pushPrefEnv({ set: [["browser.ml.chat.enabled", true]] });
  is(sidebar.toolButtons.length, origCount + 1, "Enabled tool added");

  await SpecialPowers.pushPrefEnv({
    set: [["browser.ml.chat.enabled", false]],
  });
  is(sidebar.toolButtons.length, origCount, "Disabled tool removed");

  await BrowserTestUtils.closeWindow(win);
});

/**
 * Check that tools pref changes happen for existing windows
 */
add_task(async function test_tool_pref_change() {
  const sidebar = document.querySelector("sidebar-main");
  await sidebar.updateComplete;

  const origCount = sidebar.toolButtons.length;
  is(origCount, 1, "Expected number of initial tools");

  const origTools = Services.prefs.getStringPref("sidebar.main.tools");
  await SpecialPowers.pushPrefEnv({
    set: [["sidebar.main.tools", origTools.replace(",bookmarks", "")]],
  });
  is(sidebar.toolButtons.length, origCount - 1, "Removed tool");

  await SpecialPowers.pushPrefEnv({ set: [["sidebar.main.tools", origTools]] });
  is(sidebar.toolButtons.length, origCount, "Restored tool");

  await SpecialPowers.pushPrefEnv({ clear: [["sidebar.main.tools"]] });
  is(sidebar.toolButtons.length, origCount + 1, "Restored default tools");
});

/**
 * Check that the new sidebar is hidden/shown automatically (without a browser restart)
 * when flipping the sidebar.revamp pref
 */
add_task(async function test_flip_revamp_pref() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const sidebar = win.document.querySelector("sidebar-main");
  await sidebar.updateComplete;

  let verticalTabs = win.document.querySelector("#vertical-tabs");
  ok(
    !BrowserTestUtils.isVisible(verticalTabs),
    "Vertical tabs slot is not visible initially"
  );
  // Open history sidebar
  await toggleSidebarPanel(win, "viewHistorySidebar");

  await SpecialPowers.pushPrefEnv({ set: [["sidebar.verticalTabs", true]] });
  ok(BrowserTestUtils.isVisible(verticalTabs), "Vertical tabs slot is visible");

  ok(sidebar, "Revamped sidebar is shown initially.");

  await SpecialPowers.pushPrefEnv({ set: [["sidebar.revamp", false]] });

  await TestUtils.waitForCondition(() => {
    let isSidebarMainShown =
      !win.document.getElementById("sidebar-main").hidden;
    let isSwitcherPanelShown =
      !win.document.getElementById("sidebar-header").hidden;
    // Vertical tabs pref should be turned off when revamp pref is turned off
    let isVerticalTabsShown = BrowserTestUtils.isVisible(verticalTabs);
    return !isSidebarMainShown && isSwitcherPanelShown && !isVerticalTabsShown;
  }, "The new sidebar is hidden and the old sidebar is shown.");

  ok(true, "The new sidebar is hidden and the old sidebar is shown.");

  await SpecialPowers.pushPrefEnv({
    set: [["sidebar.revamp", true]],
  });
  await sidebar.updateComplete;
  await TestUtils.waitForCondition(() => {
    let isSidebarMainShown = !document.getElementById("sidebar-main").hidden;
    let isSwitcherPanelShown =
      !win.document.getElementById("sidebar-header").hidden;
    return isSidebarMainShown && !isSwitcherPanelShown;
  }, "The old sidebar is hidden and the new sidebar is shown.");

  ok(true, "The old sidebar is hidden and the new sidebar is shown.");
  await BrowserTestUtils.closeWindow(win);
});

/**
 * Check that conditional sidebar tools hide if open on pref change
 */
add_task(async function test_conditional_tools() {
  const COMMAND_ID = "viewGenaiChatSidebar";
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const { SidebarController } = win;
  const sidebar = win.document.querySelector("sidebar-main");
  await sidebar.updateComplete;

  await SpecialPowers.pushPrefEnv({ set: [["browser.ml.chat.enabled", true]] });

  await SidebarController.show(COMMAND_ID);

  await TestUtils.waitForCondition(() => {
    return (
      SidebarController.isOpen && SidebarController.currentID == COMMAND_ID
    );
  }, "The sidebar was opened.");

  ok(true, "Conditional sidebar is shown.");

  await SpecialPowers.pushPrefEnv({
    set: [["browser.ml.chat.enabled", false]],
  });

  await TestUtils.waitForCondition(() => {
    return !SidebarController.isOpen;
  }, "The sidebar is hidden.");

  ok(true, "Conditional sidebar is hidden after the pref change.");

  await BrowserTestUtils.closeWindow(win);
});
