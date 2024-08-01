/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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
    await BrowserTestUtils.waitForCondition(() => {
      let toggledTool = win.SidebarController.toolsAndExtensions.get(
        toolInput.name
      );
      return toggledTool.disabled === !toolDisabledInitialState;
    }, `The entrypoint for ${toolInput.name} has been ${toolDisabledInitialState ? "enabled" : "disabled"} in the sidebar.`);
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
