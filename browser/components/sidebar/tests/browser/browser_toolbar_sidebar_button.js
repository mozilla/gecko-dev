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
    set: [[SIDEBAR_BUTTON_INTRODUCED_PREF, false]],
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
  await SpecialPowers.pushPrefEnv({
    set: [[SIDEBAR_VISIBILITY_PREF, "always-show"]],
  });
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const {
    SidebarController: { sidebarMain, toolbarButton },
  } = win;

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
  };

  info("Check default expanded state.");
  await checkExpandedState(false);

  info("Toggle expanded state via toolbar button.");
  EventUtils.synthesizeMouseAtCenter(toolbarButton, {}, win);
  await checkExpandedState(true);
  EventUtils.synthesizeMouseAtCenter(toolbarButton, {}, win);
  await checkExpandedState(false);

  info("Collapse the sidebar by loading a tool.");
  sidebarMain.expanded = true;
  await sidebarMain.updateComplete;
  const toolButton = sidebarMain.toolButtons[0];
  EventUtils.synthesizeMouseAtCenter(toolButton, {}, win);
  await checkExpandedState(false);

  info("Restore the sidebar back to its previous state.");
  EventUtils.synthesizeMouseAtCenter(toolButton, {}, win);
  await checkExpandedState(true);

  info("Load and unload a tool with the sidebar collapsed to begin with.");
  sidebarMain.expanded = false;
  await sidebarMain.updateComplete;
  EventUtils.synthesizeMouseAtCenter(toolButton, {}, win);
  await checkExpandedState(false);
  EventUtils.synthesizeMouseAtCenter(toolButton, {}, win);
  await checkExpandedState(false);

  info("Check expanded state on a new window.");
  sidebarMain.expanded = true;
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
});
