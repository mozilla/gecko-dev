/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(() => SpecialPowers.pushPrefEnv({ set: [["sidebar.revamp", true]] }));

async function showCustomizePanel(win) {
  await win.SidebarController.show("viewCustomizeSidebar");
  const document = win.SidebarController.browser.contentDocument;
  return TestUtils.waitForCondition(async () => {
    const component = document.querySelector("sidebar-customize");
    if (!component?.positionInputs) {
      return false;
    }
    return component;
  }, "Customize panel is shown.");
}

add_task(async function test_customize_sidebar_actions() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const { document } = win;
  await toggleSidebarPanel(win, "viewCustomizeSidebar");
  let customizeDocument = win.SidebarController.browser.contentDocument;
  const customizeComponent =
    customizeDocument.querySelector("sidebar-customize");
  const sidebar = document.querySelector("sidebar-main");
  let toolEntrypointsCount = sidebar.toolButtons.length;
  let checkedInputs = Array.from(customizeComponent.toolInputs).filter(
    input => input.checked
  );
  is(
    checkedInputs.length,
    toolEntrypointsCount,
    `${toolEntrypointsCount} inputs to toggle Firefox Tools are shown in the Customize Menu.`
  );
  is(
    customizeComponent.toolInputs.length,
    3,
    "Three default tools are shown in the customize menu"
  );
  let bookmarksInput = Array.from(customizeComponent.toolInputs).find(
    input => input.name === "viewBookmarksSidebar"
  );
  ok(
    !bookmarksInput.checked,
    "The bookmarks input is unchecked initally as Bookmarks are disabled initially."
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
    toolInput.click();
    await BrowserTestUtils.waitForCondition(() => {
      let toggledTool = win.SidebarController.toolsAndExtensions.get(
        toolInput.name
      );
      return toggledTool.disabled === toolDisabledInitialState;
    }, `The entrypoint for ${toolInput.name} has been ${toolDisabledInitialState ? "disabled" : "enabled"} in the sidebar.`);
    toolEntrypointsCount = sidebar.toolButtons.length;
    checkedInputs = Array.from(customizeComponent.toolInputs).filter(
      input => input.checked
    );
    is(
      toolEntrypointsCount,
      checkedInputs.length,
      `The button for the ${toolInput.name} entrypoint has been ${
        toolDisabledInitialState ? "removed" : "added"
      }.`
    );
    // Check ordering
    if (!toolDisabledInitialState) {
      is(
        sidebar.toolButtons[sidebar.toolButtons.length - 1].getAttribute(
          "view"
        ),
        toolInput.name,
        `The button for the ${toolInput.name} entrypoint has been added back to the end of the list of tools/extensions entrypoints`
      );
    }
  }

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_customize_not_added_in_menubar() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  const { document } = win;
  if (document.hasPendingL10nMutations) {
    await BrowserTestUtils.waitForEvent(document, "L10nMutationsFinished");
  }
  let sidebarsMenu = document.getElementById("viewSidebarMenu");
  let menuItems = sidebarsMenu.querySelectorAll("menuitem");
  ok(
    !Array.from(menuItems).find(menuitem =>
      menuitem.getAttribute("label").includes("Customize")
    ),
    "The View > Sidebars menu doesn't include any option for 'customize'."
  );

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_manage_preferences_navigation() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const { SidebarController } = win;
  const { contentWindow } = SidebarController.browser;
  const sidebar = document.querySelector("sidebar-main");
  ok(sidebar, "Sidebar is shown.");
  await sidebar.updateComplete;
  await toggleSidebarPanel(win, "viewCustomizeSidebar");
  let customizeDocument = win.SidebarController.browser.contentDocument;
  const customizeComponent =
    customizeDocument.querySelector("sidebar-customize");
  let manageSettings =
    customizeComponent.shadowRoot.getElementById("manage-settings");

  EventUtils.synthesizeMouseAtCenter(manageSettings, {}, contentWindow);
  await BrowserTestUtils.waitForCondition(
    () =>
      win.gBrowser.selectedTab.linkedBrowser.currentURI.spec ==
      "about:preferences",
    "Navigated to about:preferences tab"
  );
  is(
    win.gBrowser.selectedTab.linkedBrowser.currentURI.spec,
    "about:preferences",
    "Manage Settings link navigates to about:preferences."
  );

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_customize_position_setting() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const { document } = win;
  const panel = await showCustomizePanel(win);
  const sidebarBox = document.getElementById("sidebar-box");
  await BrowserTestUtils.waitForCondition(
    () => BrowserTestUtils.isVisible(sidebarBox),
    "Sidebar panel is visible"
  );
  const [positionLeft, positionRight] = panel.positionInputs;
  ok(positionLeft.checked, "The sidebar positioned on the left by default.");
  is(
    sidebarBox.style.order,
    "2",
    "Sidebar box should have an order of 2 when on the left"
  );
  EventUtils.synthesizeMouseAtCenter(
    positionRight,
    {},
    win.SidebarController.browser.contentWindow
  );
  ok(positionRight.checked, "Sidebar is positioned on the right");

  const newWin = await BrowserTestUtils.openNewBrowserWindow();
  const newPanel = await showCustomizePanel(newWin);
  const newSidebarBox = newWin.document.getElementById("sidebar-box");
  await BrowserTestUtils.waitForCondition(
    () => BrowserTestUtils.isVisible(newSidebarBox),
    "Sidebar panel is visible"
  );
  const [, newPositionRight] = newPanel.positionInputs;
  ok(newPositionRight.checked, "Position setting persists.");
  is(
    newSidebarBox.style.order,
    "4",
    "Sidebar box should have an order of 4 when on the right"
  );

  await BrowserTestUtils.closeWindow(win);
  await BrowserTestUtils.closeWindow(newWin);
  Services.prefs.clearUserPref("sidebar.position_start");
});
