/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);
const { SessionStoreTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SessionStoreTestUtils.sys.mjs"
);
const { NonPrivateTabs } = ChromeUtils.importESModule(
  "resource:///modules/OpenTabs.sys.mjs"
);

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["sidebar.verticalTabs", false],
      ["sidebar.visibility", "always-show"],
    ],
  });
  Services.telemetry.clearScalars();
  SessionStoreTestUtils.init(this, window);
});
registerCleanupFunction(async () => {
  await SpecialPowers.popPrefEnv();
  while (gBrowser.tabs.length > 1) {
    BrowserTestUtils.removeTab(gBrowser.tabs.at(-1));
  }
  NonPrivateTabs.stop();
});

function getTelemetryScalars(names) {
  return TestUtils.waitForCondition(
    () => {
      const scalars = TelemetryTestUtils.getProcessScalars("parent");
      return names.every(name => Object.hasOwn(scalars, name)) && scalars;
    },
    `Scalars are present in Telemetry data: ${names.join(", ")}`
  );
}

function checkTelemetryScalar(name, value) {
  return TestUtils.waitForCondition(() => {
    const scalars = TelemetryTestUtils.getProcessScalars("parent");
    return scalars[name] == value;
  }, `Scalar ${name} has value: ${value}`);
}

function getExpectedElements(win, tabstripOrientation = "horizontal") {
  const sizeMode = win.document.documentElement.getAttribute("sizemode");
  let selectors;

  // NOTE: CustomTitlebar behaviour isn't under test here. We just want to assert on
  // the right stuff being visible whatever the case for the given window.

  if (tabstripOrientation == "horizontal") {
    selectors = ["#TabsToolbar"];

    if (win.CustomTitlebar.enabled) {
      selectors.push("#TabsToolbar .titlebar-buttonbox-container");
      if (sizeMode == "normal") {
        selectors.push("#TabsToolbar .titlebar-spacer");
      }
    }
    return selectors;
  }

  selectors = ["#vertical-tabs"];
  if (win.CustomTitlebar.enabled) {
    selectors.push("#nav-bar .titlebar-buttonbox-container");
  }
  return selectors;
}

add_task(async function test_toggle_vertical_tabs() {
  const sidebar = document.querySelector("sidebar-main");
  ok(sidebar, "Sidebar is shown.");

  let tabStrip = document.getElementById("tabbrowser-tabs");
  let defaultTabstripParent = document.getElementById(
    "TabsToolbar-customization-target"
  );
  let verticalTabs = document.querySelector("#vertical-tabs");
  info(
    `toolbars collapsed: \n${Array.from(
      document.querySelectorAll("#navigator-toolbox > toolbar")
    )
      .map(t => `  ${t.id}: ${t.collapsed}`)
      .join("\n")}`
  );
  info(`sizemode: ${document.documentElement.getAttribute("sizemode")}`);
  info(
    `customtitlebar: ${document.documentElement.getAttribute("customtitlebar")}`
  );

  const expectedElementsWhenHorizontal = getExpectedElements(
    window,
    "horizontal"
  );
  const expectedElementsWhenVertical = getExpectedElements(window, "vertical");

  for (let selector of expectedElementsWhenHorizontal) {
    let elem = document.querySelector(selector);
    ok(
      elem && BrowserTestUtils.isVisible(elem),
      `${selector} exists and is visible`
    );
  }
  for (let selector of expectedElementsWhenVertical) {
    let elem = document.querySelector(selector);
    ok(
      elem && BrowserTestUtils.isHidden(elem),
      `${selector} exists and is hidden`
    );
  }

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

  for (let selector of expectedElementsWhenVertical) {
    let elem = document.querySelector(selector);
    ok(
      elem && BrowserTestUtils.isVisible(elem),
      `${selector} exists and is visible: ${!!elem}, ${
        elem && BrowserTestUtils.isVisible(elem)
      }`
    );
  }
  for (let selector of expectedElementsWhenHorizontal) {
    let elem = document.querySelector(selector);
    ok(
      elem && BrowserTestUtils.isHidden(elem),
      `${selector} exists and is hidden: ${!!elem}, ${
        elem && BrowserTestUtils.isHidden(elem)
      }`
    );
  }

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

  let keyedScalars = TelemetryTestUtils.getProcessScalars("parent", true);
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
    450,
    "Container should extend far beyond the last tab."
  );

  // We are testing click events on the tabstrip itself, rather than on any
  // specific button. Disable a11y checks for this portion.
  AccessibilityUtils.setEnv({
    mustHaveAccessibleRule: false,
  });

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

  AccessibilityUtils.resetEnv();

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

  let newTabButton = document.getElementById("tabs-newtab-button");
  info("Open a new tab using the new tab button.");
  EventUtils.synthesizeMouseAtCenter(newTabButton, {});
  is(gBrowser.tabs.length, 5, "Tabstrip now has six tabs");

  // Middle click on new tab button should also open a new tab.
  info("Open a new tab middle clicking the new tab button.");
  // Make sure there is something in the clipboard that can be opened.
  SpecialPowers.clipboardCopyString("about:blank");
  EventUtils.synthesizeMouseAtCenter(newTabButton, { button: 1 });
  is(gBrowser.tabs.length, 6, "Tabstrip now has five tabs");

  keyedScalars = TelemetryTestUtils.getProcessScalars("parent", true);
  TelemetryTestUtils.assertKeyedScalar(
    keyedScalars,
    "browser.ui.interaction.vertical_tabs_container",
    "tabs-newtab-button",
    1
  );

  await checkTelemetryScalar(
    "browser.engagement.max_concurrent_vertical_tab_count",
    6
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
    6
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

add_task(async function test_vertical_tabs_overflow() {
  const numTabs = 50;
  const winData = {
    tabs: Array.from({ length: numTabs }, (_, i) => ({
      entries: [
        {
          url: `data:,Tab${i}`,
          triggeringPrincipal_base64: E10SUtils.SERIALIZED_SYSTEMPRINCIPAL,
        },
      ],
    })),
    selected: numTabs,
  };
  const browserState = { windows: [winData] };

  // use Session restore to batch-open tabs
  info(`Restoring to browserState: ${JSON.stringify(browserState, null, 2)}`);
  await SessionStoreTestUtils.promiseBrowserState(browserState);
  info("Windows and tabs opened, waiting for readyWindowsPromise");
  await NonPrivateTabs.readyWindowsPromise;
  info("readyWindowsPromise resolved");

  info("Open a new tab using the new tab button.");
  const newTabButton = document.getElementById("vertical-tabs-newtab-button");
  ok(
    BrowserTestUtils.isVisible(newTabButton),
    "New tab button is visible while tabs are overflowing."
  );
  EventUtils.synthesizeMouseAtCenter(newTabButton, {});

  is(
    gBrowser.tabs.length,
    numTabs + 1,
    `Tabstrip now has ${numTabs + 1} tabs.`
  );
  const keyedScalars = TelemetryTestUtils.getProcessScalars("parent", true);
  TelemetryTestUtils.assertKeyedScalar(
    keyedScalars,
    "browser.ui.interaction.vertical_tabs_container",
    "vertical-tabs-newtab-button",
    1
  );
});

add_task(async function test_vertical_tabs_expanded() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["sidebar.revamp", true],
      ["sidebar.verticalTabs", true],
    ],
  });
  await SidebarController.setUIState({ expanded: true });

  info("Disable revamped sidebar.");
  Services.prefs.setBoolPref("sidebar.revamp", false);
  await TestUtils.waitForCondition(
    () => BrowserTestUtils.isHidden(document.getElementById("sidebar-main")),
    "Sidebar launcher is hidden."
  );

  info("Enable revamped sidebar and vertical tabs.");
  Services.prefs.setBoolPref("sidebar.revamp", true);
  Services.prefs.setBoolPref("sidebar.verticalTabs", true);
  await TestUtils.waitForCondition(
    () => BrowserTestUtils.isVisible(document.getElementById("sidebar-main")),
    "Sidebar launcher is shown."
  );
  ok(
    gBrowser.tabContainer.hasAttribute("expanded"),
    "Tab container is expanded."
  );

  await SpecialPowers.popPrefEnv();
});
