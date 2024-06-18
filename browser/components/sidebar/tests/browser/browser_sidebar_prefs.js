/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(() => SpecialPowers.pushPrefEnv({ set: [["sidebar.revamp", true]] }));

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
    "history,syncedtabs",
    "Default tools should be history and syncedtabs"
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

  is(
    Services.prefs.getStringPref("sidebar.main.tools"),
    "bookmarks",
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
    "bookmarks",
    "Only bookmarks should be present in the pref"
  );

  await BrowserTestUtils.closeWindow(newWin);
});
