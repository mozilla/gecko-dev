/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [["sidebar.verticalTabs", false]],
  });
  Services.telemetry.clearScalars();
});
registerCleanupFunction(async () => {
  await SpecialPowers.popPrefEnv();
  while (gBrowser.tabs.length > 1) {
    BrowserTestUtils.removeTab(gBrowser.tabs.at(-1));
  }
});

function getTelemetryScalars(names) {
  return TestUtils.waitForCondition(() => {
    const scalars = TelemetryTestUtils.getProcessScalars("parent");
    return names.every(name => Object.hasOwn(scalars, name)) && scalars;
  }, `Scalars are present in Telemetry data: ${names.join(", ")}`);
}

function checkTelemetryScalar(name, value) {
  return TestUtils.waitForCondition(() => {
    const scalars = TelemetryTestUtils.getProcessScalars("parent");
    return scalars[name] == value;
  }, `Scalar ${name} has the correct value.`);
}

add_task(async function test_toggle_vertical_tabs() {
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
  is(gBrowser.tabs.length, 1, "Tabstrip now has one tab");

  // make sure the tab context menu still works
  const contextMenu = document.getElementById("tabContextMenu");
  gBrowser.selectedTab.focus();
  EventUtils.synthesizeMouseAtCenter(gBrowser.selectedTab, {
    type: "contextmenu",
    button: 2,
  });

  info("Open a new tab using the context menu.");
  await openAndWaitForContextMenu(contextMenu, gBrowser.selectedTab, () => {
    document.getElementById("context_openANewTab").click();
  });

  const keyedScalars = TelemetryTestUtils.getProcessScalars("parent", true);
  TelemetryTestUtils.assertKeyedScalar(
    keyedScalars,
    "browser.ui.interaction.tabs_context_entrypoint",
    "vertical-tabs-container",
    1
  );

  let scalars = await getTelemetryScalars([
    "browser.engagement.vertical_tab_open_event_count",
  ]);
  TelemetryTestUtils.assertScalar(
    scalars,
    "browser.engagement.vertical_tab_open_event_count",
    1
  );
  await checkTelemetryScalar(
    "browser.engagement.max_concurrent_vertical_tab_count",
    2
  );

  info("Pin a tab using the context menu.");
  await openAndWaitForContextMenu(contextMenu, gBrowser.selectedTab, () => {
    document.getElementById("context_pinTab").click();
  });

  scalars = await getTelemetryScalars([
    "browser.engagement.max_concurrent_vertical_tab_pinned_count",
    "browser.engagement.vertical_tab_pinned_event_count",
  ]);
  TelemetryTestUtils.assertScalar(
    scalars,
    "browser.engagement.max_concurrent_vertical_tab_pinned_count",
    1
  );
  TelemetryTestUtils.assertScalar(
    scalars,
    "browser.engagement.vertical_tab_pinned_event_count",
    1
  );

  is(gBrowser.tabs.length, 2, "Tabstrip now has two tabs");

  let tabRect = gBrowser.selectedTab.getBoundingClientRect();
  let containerRect = gBrowser.tabContainer.getBoundingClientRect();

  Assert.greater(
    containerRect.bottom - tabRect.bottom,
    500,
    "Container should extend far beyond the last tab."
  );

  // Synthesize a double click 100px below the last tab
  EventUtils.synthesizeMouseAtPoint(
    containerRect.left + containerRect.width / 2,
    tabRect.bottom + 100,
    { clickCount: 1 }
  );
  EventUtils.synthesizeMouseAtPoint(
    containerRect.left + containerRect.width / 2,
    tabRect.bottom + 100,
    { clickCount: 2 }
  );

  is(gBrowser.tabs.length, 3, "Tabstrip now has three tabs");

  tabRect = gBrowser.selectedTab.getBoundingClientRect();

  // Middle click should also open a new tab.
  EventUtils.synthesizeMouseAtPoint(
    containerRect.left + containerRect.width / 2,
    tabRect.bottom + 100,
    { button: 1 }
  );

  is(gBrowser.tabs.length, 4, "Tabstrip now has four tabs");

  const toolbarContextMenu = document.getElementById("toolbar-context-menu");
  EventUtils.synthesizeMouseAtPoint(
    containerRect.left + containerRect.width / 2,
    tabRect.bottom + 100,
    {
      type: "contextmenu",
      button: 2,
    }
  );

  await openAndWaitForContextMenu(
    toolbarContextMenu,
    gBrowser.selectedTab,
    () => {
      ok(
        document.getElementById("toolbar-context-customize").hidden,
        "Customize menu item should be hidden"
      );
      ok(
        !document.getElementById("toggle_PersonalToolbar"),
        "Bookmarks menu item should not be present"
      );
      ok(
        !document.getElementById("toolbar-context-reloadSelectedTab").hidden,
        "Reload tab item should be visible"
      );
      ok(
        !document.getElementById("toolbar-context-undoCloseTab").hidden,
        "Undo close tab item should be visible"
      );
    }
  );

  await checkTelemetryScalar(
    "browser.engagement.max_concurrent_vertical_tab_count",
    4
  );

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

  scalars = await getTelemetryScalars([
    "browser.engagement.max_concurrent_tab_count",
    "browser.engagement.max_concurrent_tab_pinned_count",
  ]);
  TelemetryTestUtils.assertScalar(
    scalars,
    "browser.engagement.max_concurrent_tab_count",
    4
  );
  TelemetryTestUtils.assertScalar(
    scalars,
    "browser.engagement.max_concurrent_tab_pinned_count",
    1
  );
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
