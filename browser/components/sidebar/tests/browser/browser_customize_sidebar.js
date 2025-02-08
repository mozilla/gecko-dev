/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

const SIDEBAR_VISIBILITY_PREF = "sidebar.visibility";
const POSITION_SETTING_PREF = "sidebar.position_start";
const TAB_DIRECTION_PREF = "sidebar.verticalTabs";

registerCleanupFunction(() => {
  Services.prefs.clearUserPref(SIDEBAR_VISIBILITY_PREF);
  Services.prefs.clearUserPref(POSITION_SETTING_PREF);
  Services.prefs.clearUserPref(TAB_DIRECTION_PREF);
});

async function showCustomizePanel(win) {
  await win.SidebarController.show("viewCustomizeSidebar");
  const document = win.SidebarController.browser.contentDocument;
  return TestUtils.waitForCondition(async () => {
    const component = document.querySelector("sidebar-customize");
    if (
      !component?.positionInput ||
      (win.SidebarController.sidebarVerticalTabsEnabled &&
        !component?.visibilityInput)
    ) {
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
    4,
    "Four default tools are shown in the customize menu"
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
    toolInput.click();
    await BrowserTestUtils.waitForCondition(
      () => {
        let toggledTool = win.SidebarController.toolsAndExtensions.get(
          toolInput.name
        );
        return toggledTool.disabled === toolDisabledInitialState;
      },
      `The entrypoint for ${toolInput.name} has been ${toolDisabledInitialState ? "disabled" : "enabled"} in the sidebar.`
    );
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
  manageSettings.querySelector("a").scrollIntoView();

  EventUtils.synthesizeMouseAtCenter(
    manageSettings.querySelector("a"),
    {},
    contentWindow
  );
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

  ok(
    !panel.positionInput.checked,
    "The sidebar positioned on the left by default."
  );
  is(
    sidebarBox.style.order,
    "3",
    "Sidebar box should have an order of 3 when on the left"
  );
  EventUtils.synthesizeMouseAtCenter(
    panel.positionInput,
    {},
    win.SidebarController.browser.contentWindow
  );
  ok(panel.positionInput.checked, "Sidebar is positioned on the right");

  const newWin = await BrowserTestUtils.openNewBrowserWindow();
  const newPanel = await showCustomizePanel(newWin);
  const newSidebarBox = newWin.document.getElementById("sidebar-box");
  await BrowserTestUtils.waitForCondition(
    () => BrowserTestUtils.isVisible(newSidebarBox),
    "Sidebar panel is visible"
  );

  ok(newPanel.positionInput.checked, "Position setting persists.");
  is(
    newSidebarBox.style.order,
    "5",
    "Sidebar box should have an order of 5 when on the right"
  );

  await BrowserTestUtils.closeWindow(win);
  await BrowserTestUtils.closeWindow(newWin);
  Services.prefs.clearUserPref("sidebar.position_start");
});

add_task(async function test_customize_visibility_setting() {
  await SpecialPowers.pushPrefEnv({
    set: [[TAB_DIRECTION_PREF, true]],
  });
  const deferredPrefChange = Promise.withResolvers();
  const prefObserver = () => deferredPrefChange.resolve();
  Services.prefs.addObserver(SIDEBAR_VISIBILITY_PREF, prefObserver);
  registerCleanupFunction(() =>
    Services.prefs.removeObserver(SIDEBAR_VISIBILITY_PREF, prefObserver)
  );

  const win = await BrowserTestUtils.openNewBrowserWindow();
  const panel = await showCustomizePanel(win);
  ok(!panel.visibilityInput.checked, "Always show is enabled by default.");
  EventUtils.synthesizeMouseAtCenter(
    panel.visibilityInput,
    {},
    win.SidebarController.browser.contentWindow
  );
  ok(panel.visibilityInput.checked, "Hide sidebar is enabled.");
  await deferredPrefChange.promise;
  const newPrefValue = Services.prefs.getStringPref(SIDEBAR_VISIBILITY_PREF);
  is(newPrefValue, "hide-sidebar", "Visibility preference updated.");

  const newWin = await BrowserTestUtils.openNewBrowserWindow();
  const newPanel = await showCustomizePanel(newWin);
  ok(newPanel.visibilityInput.checked, "Visibility setting persists.");

  await BrowserTestUtils.closeWindow(win);
  await BrowserTestUtils.closeWindow(newWin);

  Services.prefs.clearUserPref(SIDEBAR_VISIBILITY_PREF);
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_vertical_tabs_setting() {
  const deferredPrefChange = Promise.withResolvers();
  const prefObserver = () => deferredPrefChange.resolve();
  Services.prefs.addObserver(TAB_DIRECTION_PREF, prefObserver);
  registerCleanupFunction(() =>
    Services.prefs.removeObserver(TAB_DIRECTION_PREF, prefObserver)
  );

  const win = await BrowserTestUtils.openNewBrowserWindow();
  const panel = await showCustomizePanel(win);
  ok(
    !panel.verticalTabsInput.checked,
    "Horizontal tabs is enabled by default."
  );
  EventUtils.synthesizeMouseAtCenter(
    panel.verticalTabsInput,
    {},
    win.SidebarController.browser.contentWindow
  );
  ok(panel.verticalTabsInput.checked, "Vertical tabs is enabled.");
  await deferredPrefChange.promise;
  const newPrefValue = Services.prefs.getBoolPref(TAB_DIRECTION_PREF);
  is(newPrefValue, true, "Vertical tabs pref updated.");

  const newWin = await BrowserTestUtils.openNewBrowserWindow();
  const newPanel = await showCustomizePanel(newWin);
  ok(newPanel.verticalTabsInput.checked, "Vertical tabs setting persists.");

  await BrowserTestUtils.closeWindow(win);
  await BrowserTestUtils.closeWindow(newWin);

  Services.prefs.clearUserPref(TAB_DIRECTION_PREF);
});

add_task(async function test_keyboard_navigation_away_from_settings_link() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const panel = await showCustomizePanel(win);
  const manageSettingsLink = panel.shadowRoot.querySelector(
    "#manage-settings a[href='about:preferences']"
  );
  manageSettingsLink.focus();

  Assert.equal(
    panel.shadowRoot.activeElement,
    manageSettingsLink,
    "Settings link is focused"
  );
  EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true }, win);
  Assert.notEqual(
    panel.shadowRoot.activeElement,
    manageSettingsLink,
    "Settings link is not focused"
  );

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_settings_synchronized_across_windows() {
  const panel = await showCustomizePanel(window);
  const { contentWindow } = SidebarController.browser;
  const newWindow = await BrowserTestUtils.openNewBrowserWindow();
  const newPanel = await showCustomizePanel(newWindow);

  info("Update vertical tabs settings.");
  EventUtils.synthesizeMouseAtCenter(
    panel.verticalTabsInput,
    {},
    contentWindow
  );
  await newPanel.updateComplete;
  ok(
    newPanel.verticalTabsInput.checked,
    "New window shows the vertical tabs setting."
  );

  info("Update visibility settings.");
  EventUtils.synthesizeMouseAtCenter(panel.visibilityInput, {}, contentWindow);
  await newPanel.updateComplete;
  ok(
    newPanel.visibilityInput.checked,
    "New window shows the updated visibility setting."
  );

  info("Update position settings.");
  EventUtils.synthesizeMouseAtCenter(panel.positionInput, {}, contentWindow);
  await newPanel.updateComplete;
  ok(
    newPanel.positionInput.checked,
    "New window shows the updated position setting."
  );

  SidebarController.hide();
  await BrowserTestUtils.closeWindow(newWindow);
});
