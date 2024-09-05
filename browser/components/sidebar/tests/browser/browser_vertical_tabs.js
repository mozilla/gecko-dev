/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(() =>
  SpecialPowers.pushPrefEnv({
    set: [["sidebar.verticalTabs", false]],
  })
);
registerCleanupFunction(() => SpecialPowers.popPrefEnv());

add_task(async function test_toggle_vertical_tabs() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  await waitForBrowserWindowActive(win);
  const { document } = win;
  const sidebar = document.querySelector("sidebar-main");
  ok(sidebar, "Sidebar is shown.");

  let tabStrip = document.getElementById("tabbrowser-tabs");
  let defaultTabstripParent = document.getElementById(
    "TabsToolbar-customization-target"
  );
  let verticalTabs = document.querySelector("#vertical-tabs");
  ok(
    !BrowserTestUtils.isVisible(verticalTabs),
    "Vertical tabs slot is not visible"
  );

  is(
    tabStrip.parentNode,
    defaultTabstripParent,
    "Tabstrip is in default horizontal position"
  );
  is(
    tabStrip.nextElementSibling.id,
    "new-tab-button",
    "Tabstrip is before the new tab button"
  );

  // flip the pref to move the tabstrip into the sidebar
  await SpecialPowers.pushPrefEnv({ set: [["sidebar.verticalTabs", true]] });
  ok(BrowserTestUtils.isVisible(verticalTabs), "Vertical tabs slot is visible");
  is(
    tabStrip.parentNode,
    verticalTabs,
    "Tabstrip is slotted into the sidebar vertical tabs container"
  );
  is(win.gBrowser.tabs.length, 1, "Tabstrip now has one tab");

  // make sure the tab context menu still works
  const contextMenu = document.getElementById("tabContextMenu");
  win.gBrowser.selectedTab.focus();
  EventUtils.synthesizeMouseAtCenter(
    win.gBrowser.selectedTab,
    {
      type: "contextmenu",
      button: 2,
    },
    win
  );

  await openAndWaitForContextMenu(contextMenu, win.gBrowser.selectedTab, () => {
    document.getElementById("context_openANewTab").click();
  });

  is(win.gBrowser.tabs.length, 2, "Tabstrip now has two tabs");

  let tabRect = win.gBrowser.selectedTab.getBoundingClientRect();
  let containerRect = win.gBrowser.tabContainer.getBoundingClientRect();

  Assert.greater(
    containerRect.bottom - tabRect.bottom,
    500,
    "Container should extend far beyond the last tab."
  );

  // Synthesize a double click 100px below the last tab
  EventUtils.synthesizeMouseAtPoint(
    containerRect.left + containerRect.width / 2,
    tabRect.bottom + 100,
    { clickCount: 1 },
    win
  );
  EventUtils.synthesizeMouseAtPoint(
    containerRect.left + containerRect.width / 2,
    tabRect.bottom + 100,
    { clickCount: 2 },
    win
  );

  is(win.gBrowser.tabs.length, 3, "Tabstrip now has three tabs");

  tabRect = win.gBrowser.selectedTab.getBoundingClientRect();

  // Middle click should also open a new tab.
  EventUtils.synthesizeMouseAtPoint(
    containerRect.left + containerRect.width / 2,
    tabRect.bottom + 100,
    { button: 1 },
    win
  );

  is(win.gBrowser.tabs.length, 4, "Tabstrip now has four tabs");

  // flip the pref to move the tabstrip horizontally
  await SpecialPowers.pushPrefEnv({ set: [["sidebar.verticalTabs", false]] });

  ok(
    !BrowserTestUtils.isVisible(verticalTabs),
    "Vertical tabs slot is not visible"
  );
  is(
    tabStrip.parentNode,
    defaultTabstripParent,
    "Tabstrip is in default horizontal position"
  );
  is(
    tabStrip.nextElementSibling.id,
    "new-tab-button",
    "Tabstrip is before the new tab button"
  );

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_enabling_vertical_tabs_enables_sidebar_revamp() {
  await SpecialPowers.pushPrefEnv({ set: [["sidebar.revamp", false]] });
  ok(
    !Services.prefs.getBoolPref("sidebar.revamp", false),
    "sidebar.revamp pref is false initially."
  );
  ok(
    !Services.prefs.getBoolPref("sidebar.verticalTabs", false),
    "sidebar.verticalTabs pref is false initially."
  );

  await SpecialPowers.pushPrefEnv({ set: [["sidebar.verticalTabs", true]] });
  ok(
    Services.prefs.getBoolPref("sidebar.verticalTabs", false),
    "sidebar.verticalTabs pref is enabled after we've enabled it."
  );
  ok(
    Services.prefs.getBoolPref("sidebar.revamp", false),
    "sidebar.revamp pref is also enabled after we've enabled vertical tabs."
  );
});
