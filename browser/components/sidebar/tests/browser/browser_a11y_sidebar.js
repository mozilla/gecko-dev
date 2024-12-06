/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let win;

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.ml.chat.enabled", true],
      ["sidebar.main.tools", "aichat,syncedtabs,history,bookmarks"],
    ],
  });
  win = await BrowserTestUtils.openNewBrowserWindow();
});

registerCleanupFunction(async () => {
  await BrowserTestUtils.closeWindow(win);
});

function isActiveElement(el) {
  return el.getRootNode().activeElement == el;
}

add_task(async function test_keyboard_navigation() {
  const { document } = win;
  const sidebar = document.querySelector("sidebar-main");
  const toolButtons = await TestUtils.waitForCondition(
    () => sidebar.toolButtons,
    "Tool buttons are shown."
  );

  toolButtons[0].focus();
  ok(isActiveElement(toolButtons[0]), "First tool button is focused.");

  info("Press Arrow Down key.");
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
  ok(isActiveElement(toolButtons[1]), "Second tool button is focused.");

  info("Press Arrow Up key.");
  EventUtils.synthesizeKey("KEY_ArrowUp", {}, win);
  ok(isActiveElement(toolButtons[0]), "First tool button is focused.");

  info("Press Enter key.");
  EventUtils.synthesizeKey("KEY_Enter", {}, win);
  await sidebar.updateComplete;
  ok(sidebar.open, "Sidebar is open.");
  is(
    sidebar.selectedView,
    toolButtons[0].getAttribute("view"),
    "Sidebar is showing the first tool."
  );
  is(
    toolButtons[0].getAttribute("aria-pressed"),
    "true",
    "aria-pressed is true for the active tool button."
  );
  is(
    toolButtons[1].getAttribute("aria-pressed"),
    "false",
    "aria-pressed is false for the inactive tool button."
  );

  info("Press Enter key again.");
  EventUtils.synthesizeKey("KEY_Enter", {}, win);
  await sidebar.updateComplete;
  ok(!sidebar.open, "Sidebar is closed.");
  is(
    toolButtons[0].getAttribute("aria-pressed"),
    "false",
    "Tool is no longer active, aria-pressed becomes false."
  );

  const customizeButton = sidebar.customizeButton;
  toolButtons[0].focus();

  info("Press Tab key.");
  EventUtils.synthesizeKey("KEY_Tab", {}, win);
  ok(isActiveElement(customizeButton), "Customize button is focused.");
});

add_task(async function test_menu_items_labeled() {
  const { document, SidebarController } = win;
  const sidebar = document.querySelector("sidebar-main");
  const allButtons = await TestUtils.waitForCondition(
    () => sidebar.allButtons,
    "All buttons are shown."
  );
  const dynamicTooltips = Object.keys(SidebarController.sidebarMain.tooltips);

  await SidebarController.setUIState({ expanded: false });
  await sidebar.updateComplete;
  for (const button of allButtons) {
    const view = button.getAttribute("view");
    const title = button.title;
    ok(title, `${view} button has a tooltip.`);
    if (dynamicTooltips.includes(view)) {
      await SidebarController.show(view);
      isnot(
        title,
        button.title,
        `${view} button has a different tooltip when the panel is open.`
      );
      SidebarController.hide();
    }
    ok(!button.hasVisibleLabel, `Collapsed ${view} button has no label.`);
  }

  await SidebarController.setUIState({ expanded: true });
  await sidebar.updateComplete;
  for (const button of allButtons) {
    const view = button.getAttribute("view");
    ok(
      button.label || button.hasVisibleLabel,
      `Expanded ${view} button has a label.`
    );
  }
});

add_task(async function test_keyboard_navigation_vertical_tabs() {
  const { document } = win;
  SpecialPowers.pushPrefEnv({
    set: [["sidebar.verticalTabs", true]],
  });
  const sidebar = document.querySelector("sidebar-main");
  const toolButtons = await TestUtils.waitForCondition(
    () => sidebar.toolButtons,
    "Tool buttons are shown."
  );
  const newTabButton = sidebar.querySelector("#tabs-newtab-button");

  win.gBrowser.tabs[0].focus();
  ok(isActiveElement(win.gBrowser.tabs[0]), "First tab is focused.");

  info("Tab to new tab button.");
  EventUtils.synthesizeKey("KEY_Tab", {}, win);
  ok(isActiveElement(newTabButton), "New tab button is focused.");

  info("Press Enter key.");
  EventUtils.synthesizeKey("KEY_Enter", {}, win);
  await TestUtils.waitForCondition(
    () => win.gBrowser.tabs.length === 2,
    "Two tabs are open."
  );

  // URL bar will be focused after opening a new tab,
  // so we need to move it back down to the new tab button
  newTabButton.focus();
  ok(isActiveElement(newTabButton), "New tab button is focused again.");

  info("Tab to get to tools.");
  EventUtils.synthesizeKey("KEY_Tab", {}, win);
  ok(isActiveElement(toolButtons[0]), "First tool button is focused.");

  info("Shift+Tab back to new tab button.");
  EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true }, win);
  ok(isActiveElement(newTabButton), "New tab button is focused.");

  await SpecialPowers.popPrefEnv();
  // clean up extra tabs
  while (win.gBrowser.tabs.length > 1) {
    BrowserTestUtils.removeTab(win.gBrowser.tabs.at(-1));
  }
});
