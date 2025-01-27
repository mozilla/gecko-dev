/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CustomizableUIInternal = CustomizableUI.getTestOnlyInternalProp(
  "CustomizableUIInternal"
);
let gAreas = CustomizableUI.getTestOnlyInternalProp("gAreas");

const SIDEBAR_BUTTON_INTRODUCED_PREF =
  "browser.toolbarbuttons.introduced.sidebar-button";
const SIDEBAR_VISIBILITY_PREF = "sidebar.visibility";

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      [SIDEBAR_BUTTON_INTRODUCED_PREF, false],
      [SIDEBAR_VISIBILITY_PREF, "always-show"],
    ],
  });
  let navbarDefaults = gAreas.get("nav-bar").get("defaultPlacements");
  let hadSavedState = !!CustomizableUI.getTestOnlyInternalProp("gSavedState");
  if (!hadSavedState) {
    CustomizableUI.setTestOnlyInternalProp("gSavedState", {
      currentVersion: CustomizableUI.getTestOnlyInternalProp("kVersion"),
      placements: {
        "nav-bar": Array.from(navbarDefaults),
      },
    });
  }

  // Normally, if the `sidebar.revamp` pref is set at startup,
  // we'll have it in the default placements for the navbar.
  // Simulate this from the test:
  let backupDefaults = Array.from(navbarDefaults);
  registerCleanupFunction(() => {
    gAreas.get("nav-bar").set("defaultPlacements", backupDefaults);
    CustomizableUI.reset();
  });
  navbarDefaults.splice(
    navbarDefaults.indexOf("stop-reload-button") + 1,
    0,
    "sidebar-button"
  );
  if (window.SidebarController.sidebarMain?.expanded) {
    info("In setup, the sidebar is currently expanded. Collapsing it");
    await window.SidebarController.initializeUIState({
      launcherExpanded: false,
    });
    await window.SidebarController.sidebarMain.updateComplete;
  }
  ok(
    BrowserTestUtils.isVisible(window.SidebarController.sidebarMain),
    "Sidebar launcher is visible at setup"
  );
});

add_task(async function test_toolbar_sidebar_button() {
  ok(
    document.getElementById("sidebar-button"),
    "Sidebar button is showing in the toolbar initially."
  );
  let gFuturePlacements =
    CustomizableUI.getTestOnlyInternalProp("gFuturePlacements");
  is(
    gFuturePlacements.size,
    0,
    "All future placements should be dealt with by now."
  );

  // Check it's actually present if we open a new window.
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const { document: newDoc } = win;
  ok(
    newDoc.getElementById("sidebar-button"),
    "Should have button in new window"
  );
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_expanded_state_for_always_show() {
  info(
    `Current window's sidebarMain.expanded: ${window.SidebarController.sidebarMain?.expanded}`
  );
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const { SidebarController, document } = win;
  const { sidebarMain, toolbarButton } = SidebarController;
  await SidebarController.promiseInitialized;
  info(`New window's sidebarMain.expanded: ${sidebarMain?.expanded}`);

  const checkExpandedState = async (
    expanded,
    component = sidebarMain,
    button = toolbarButton
  ) => {
    await TestUtils.waitForCondition(
      () => Boolean(component.expanded) == expanded,
      expanded ? "Sidebar is expanded." : "Sidebar is collapsed."
    );
    await TestUtils.waitForCondition(
      () => Boolean(button.checked) == expanded,
      expanded
        ? "Toolbar button is highlighted."
        : "Toolbar button is not highlighted."
    );
    Assert.deepEqual(
      document.l10n.getAttributes(button),
      {
        id: expanded
          ? "sidebar-widget-collapse-sidebar2"
          : "sidebar-widget-expand-sidebar2",
        args:
          AppConstants.platform === "macosx"
            ? { shortcut: "⌃Z" }
            : { shortcut: "Alt+Ctrl+Z" },
      },
      "Toolbar button has the correct tooltip."
    );
    await TestUtils.waitForCondition(
      () => button.hasAttribute("expanded") == expanded,
      expanded
        ? "Toolbar button expanded attribute is present."
        : "Toolbar button expanded attribute is absent."
    );
  };

  info("Check default expanded state.");
  await checkExpandedState(false);
  ok(
    BrowserTestUtils.isVisible(sidebarMain),
    "The sidebar launcher is visible"
  );
  ok(
    !toolbarButton.hasAttribute("checked"),
    "The toolbar button is not checked."
  );
  info("Toggle expanded state via toolbar button.");
  EventUtils.synthesizeMouseAtCenter(toolbarButton, {}, win);
  await checkExpandedState(true);
  ok(toolbarButton.hasAttribute("checked"), "The toolbar button is checked.");
  EventUtils.synthesizeMouseAtCenter(toolbarButton, {}, win);
  await checkExpandedState(false);

  info("Collapse the sidebar by loading a tool.");
  await SidebarController.initializeUIState({ launcherExpanded: true });
  await sidebarMain.updateComplete;
  const toolButton = sidebarMain.toolButtons[0];
  EventUtils.synthesizeMouseAtCenter(toolButton, {}, win);
  await checkExpandedState(false);

  info("Restore the sidebar back to its previous state.");
  EventUtils.synthesizeMouseAtCenter(toolButton, {}, win);
  await checkExpandedState(true);

  info("Load and unload a tool with the sidebar collapsed to begin with.");
  await SidebarController.initializeUIState({ launcherExpanded: false });
  await sidebarMain.updateComplete;
  EventUtils.synthesizeMouseAtCenter(toolButton, {}, win);
  await checkExpandedState(false);
  EventUtils.synthesizeMouseAtCenter(toolButton, {}, win);
  await checkExpandedState(false);

  info("Check expanded state on a new window.");
  await SidebarController.initializeUIState({ launcherExpanded: true });
  await sidebarMain.updateComplete;
  const newWin = await BrowserTestUtils.openNewBrowserWindow();
  await checkExpandedState(
    true,
    newWin.SidebarController.sidebarMain,
    newWin.SidebarController.toolbarButton
  );

  await BrowserTestUtils.closeWindow(win);
  await BrowserTestUtils.closeWindow(newWin);
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_states_for_hide_sidebar() {
  await SpecialPowers.pushPrefEnv({
    set: [[SIDEBAR_VISIBILITY_PREF, "hide-sidebar"]],
  });
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const { SidebarController } = win;
  const { sidebarContainer, sidebarMain, toolbarButton } = SidebarController;

  const checkStates = async (
    { hidden, expanded },
    container = sidebarContainer,
    component = sidebarMain,
    button = toolbarButton
  ) => {
    await TestUtils.waitForCondition(
      () => container.hidden == hidden,
      "Hidden state is correct."
    );
    await TestUtils.waitForCondition(
      () => component.expanded == expanded,
      "Expanded state is correct."
    );
    await TestUtils.waitForCondition(
      () => button.checked == !hidden,
      "Toolbar button state is correct."
    );
    Assert.deepEqual(
      document.l10n.getAttributes(button),
      {
        id: hidden
          ? "sidebar-widget-show-sidebar2"
          : "sidebar-widget-hide-sidebar2",
        args:
          AppConstants.platform === "macosx"
            ? { shortcut: "⌃Z" }
            : { shortcut: "Alt+Ctrl+Z" },
      },
      "Toolbar button has the correct tooltip."
    );
    await TestUtils.waitForCondition(
      () => button.hasAttribute("expanded") == expanded,
      expanded
        ? "Toolbar button expanded attribute is present."
        : "Toolbar button expanded attribute is absent."
    );
  };

  // Hide the sidebar
  EventUtils.synthesizeMouseAtCenter(toolbarButton, {}, win);
  info("Check default hidden state.");
  await checkStates({ hidden: true, expanded: false });

  info("Show expanded sidebar using the toolbar button.");
  EventUtils.synthesizeMouseAtCenter(toolbarButton, {}, win);
  await checkStates({ hidden: false, expanded: true });

  info("Collapse the sidebar by loading a tool.");
  const toolButton = sidebarMain.toolButtons[0];
  EventUtils.synthesizeMouseAtCenter(toolButton, {}, win);
  await checkStates({ hidden: false, expanded: false });

  info("Restore the sidebar back to its previous state.");
  EventUtils.synthesizeMouseAtCenter(toolButton, {}, win);
  await checkStates({ hidden: false, expanded: true });

  info("Close a panel using the toolbar button.");
  EventUtils.synthesizeMouseAtCenter(toolButton, {}, win);
  ok(SidebarController.isOpen, "Panel is open.");
  EventUtils.synthesizeMouseAtCenter(toolbarButton, {}, win);
  ok(!SidebarController.isOpen, "Panel is closed.");
  await checkStates({ hidden: true, expanded: true });

  info("Check states on a new window.");
  EventUtils.synthesizeMouseAtCenter(toolbarButton, {}, win);
  await checkStates({ hidden: false, expanded: true });
  const newWin = await BrowserTestUtils.openNewBrowserWindow();
  await checkStates(
    { hidden: false, expanded: true },
    newWin.SidebarController.sidebarContainer,
    newWin.SidebarController.sidebarMain,
    newWin.SidebarController.toolbarButton
  );

  await BrowserTestUtils.closeWindow(win);
  await BrowserTestUtils.closeWindow(newWin);
  await SpecialPowers.popPrefEnv();
}).skip(); //bug 1896421

add_task(async function test_sidebar_button_runtime_pref_enabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["sidebar.revamp", false]],
  });
  let button = document.getElementById("sidebar-button");
  Assert.ok(
    BrowserTestUtils.isVisible(button),
    "The sidebar button is visible"
  );

  CustomizableUI.removeWidgetFromArea("sidebar-button");
  Assert.ok(
    BrowserTestUtils.isHidden(button),
    "The sidebar button is not visible after being removed"
  );

  // rever the pref change, this should cause the button to be placed in the nav-bar
  await SpecialPowers.popPrefEnv();
  button = document.getElementById("sidebar-button");
  Assert.ok(
    BrowserTestUtils.isVisible(button),
    "The sidebar button is visible again when the pref is flipped"
  );

  let widgetPlacement = CustomizableUI.getPlacementOfWidget("sidebar-button");
  Assert.equal(
    widgetPlacement.area,
    CustomizableUI.AREA_NAVBAR,
    "The sidebar button is in the nav-bar"
  );

  // When the button was removed, "hide-sidebar" was set automatically. Revert for the next test.
  // Expanded is the default when "hide-sidebar" is set - click the button to revert to collapsed for the next test.
  await SpecialPowers.pushPrefEnv({
    set: [[SIDEBAR_VISIBILITY_PREF, "always-show"]],
  });
  const sidebar = document.querySelector("sidebar-main");
  button.click();
  Assert.ok(!sidebar.expanded, "Sidebar collapsed by click");
});

/**
 * Check that keyboard shortcut toggles sidebar
 */
add_task(async function test_keyboard_shortcut() {
  const sidebar = document.querySelector("sidebar-main");
  const key = document.getElementById("toggleSidebarKb");

  Assert.ok(!sidebar.expanded, "Sidebar collapsed by default");

  key.doCommand();

  Assert.ok(sidebar.expanded, "Sidebar expanded with keyboard");

  key.doCommand();

  Assert.ok(!sidebar.expanded, "Closed sidebar with keyboard");
  const events = Glean.sidebar.keyboardShortcut.testGetValue();
  Assert.equal(events.length, 2, "Got 2 keyboard events");
  Assert.equal(
    events[0].extra.panel,
    "",
    "No sidebar panels opened when expanding via keyboard shortcut"
  );
  Assert.equal(
    events[0].extra.opened,
    "true",
    "Glean event recorded that sidebar was expanded/shown with keyboard shortcut"
  );
  Assert.equal(
    events[1].extra.opened,
    "false",
    "Glean event recorded that sidebar was collapsed/hidden with keyboard shortcut"
  );

  Services.fog.testResetFOG();
});
