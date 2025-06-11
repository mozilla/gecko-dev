/**
 * Test reordering the tabs in the Tab Manager, moving the tab between the
 * Tab Manager and tab bar.
 */

"use strict";

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

const URL1 = "data:text/plain,tab1";
const URL2 = "data:text/plain,tab2";
const URL3 = "data:text/plain,tab3";
const URL4 = "data:text/plain,tab4";
const URL5 = "data:text/plain,tab5";

const TAB_GROUP_1 = "tab-group-1";
const TAB_GROUP_2 = "tab-group-2";

/**
 * @param {(string|number)[]} order
 * @param {(string|number)[]} expected
 * @param {string} message
 */
function assertOrder(order, expected, message) {
  is(
    JSON.stringify(order),
    JSON.stringify(expected),
    `The order of the tabs ${message}`
  );
}

/**
 * Returns a numeric ID that represents a tab. The ID is based on the URL, e.g.
 * a tab with the URL `data:text/plain,tab3` has an ID of `3`.
 *
 * @see URL1 through URL5
 * @param {MozTabbrowserTab} tab
 * @returns {string|number}
 *   Numeric tab ID, or `"unknown"` if the ID can't be determined.
 */
function getTabIdFromTab(tab) {
  const url = tab.linkedBrowser.currentURI.spec ?? "";
  const m = url.match(/^data:text\/plain,tab(\d)/);
  if (m) {
    return parseInt(m[1]);
  }
  return "unknown";
}

/**
 * Returns a string ID that represents a tab group.
 *
 * @param {MozTabbrowserTabGroup} tabGroup
 * @returns {string}
 */
function getTabGroupIdFromTabGroup(tabGroup) {
  return tabGroup.id;
}

/**
 * @see TabsList.sys.mjs#getTabFromRow
 * @param {XulToolbarItem} row
 * @returns {MozTabbrowserTab}
 */
function tabOf(row) {
  return row._tab;
}

/**
 * @see TabsList.sys.mjs#getTabGroupFromRow
 * @param {XulToolbarItem} row
 * @returns {MozTabbrowserTabGroup}
 */
function tabGroupOf(row) {
  return row._tabGroup;
}

/**
 * @param {XulToolbarItem} row
 * @returns {string|number|"unknown"}
 *   A unique identifier for a tab or tab group represented by `row`:
 *   - `string` for a tab group ID (see TAB_GROUP_*).
 *   - `number` for the URL text data ID of a tab (see URL*).
 *   - "unknown" if the row didn't have tab or tab group data for some reason.
 */
function getRowId(row) {
  if (row.getAttribute("row-variant") == "tab") {
    return getTabIdFromTab(tabOf(row));
  } else if (row.getAttribute("row-variant") == "tab-group") {
    return getTabGroupIdFromTabGroup(tabGroupOf(row));
  }
  return "unknown";
}

/**
 * Returns an ordered set of IDs describing the state of the tabs list menu.
 * This state should always match the real state of the tab strip.
 *
 * @param {Element} containerNode
 *   The `containerNode` configured for an instance of `TabsList`, i.e. the root
 *   node of the menu of tabs.
 * @returns {(string|number)[]}
 */
function getTabsListOrderedIds(containerNode) {
  return [...containerNode.querySelectorAll("toolbaritem")].map(row =>
    getRowId(row)
  );
}

/**
 * Returns an ordered set of IDs describing the real state of the tab strip as
 * reported by Tabbrowser. The `TabsList` state should always match this state.
 *
 * @param {Window} win
 * @returns {(string|number)[]}
 */
function getTabStripOrderedIds(win) {
  return win.gBrowser.tabContainer.ariaFocusableItems.map(tabStripItem => {
    if (win.gBrowser.isTab(tabStripItem)) {
      return getTabIdFromTab(tabStripItem);
    }
    if (win.gBrowser.isTabGroupLabel(tabStripItem)) {
      return tabStripItem.group.id;
    }
    return "unknown";
  });
}

async function testWithNewWindow(func) {
  const newWindow = await BrowserTestUtils.openNewBrowserWindow();

  await Promise.all([
    addTabTo(newWindow.gBrowser, URL1),
    addTabTo(newWindow.gBrowser, URL2),
    addTabTo(newWindow.gBrowser, URL3),
    addTabTo(newWindow.gBrowser, URL4),
    addTabTo(newWindow.gBrowser, URL5),
  ]);

  newWindow.gTabsPanel.init();

  const button = newWindow.document.getElementById("alltabs-button");

  const allTabsView = newWindow.document.getElementById(
    "allTabsMenu-allTabsView"
  );
  const allTabsPopupShownPromise = BrowserTestUtils.waitForEvent(
    allTabsView,
    "ViewShown"
  );
  button.click();
  await allTabsPopupShownPromise;

  await func(newWindow);

  await BrowserTestUtils.closeWindow(newWindow);
}

/**
 * Virtually drag and drop a `source` element onto the `target` element,
 * offset by `clientX`, `clientY` pixels from the top-left of the viewport.
 *
 * @param {Element} source
 * @param {Element} target
 * @param {number} clientX
 * @param {number} clientY
 * @param {Window} win
 */
async function drop(source, target, clientX, clientY, win) {
  EventUtils.synthesizeDrop(source, target, null, "move", win, win, {
    clientX,
    clientY,
  });
  await win.gTabsPanel.allTabsPanel.domRefreshComplete;
}

/**
 * Virtually drag and drop one tabs list row after another.
 *
 * @param {XulToolbarItem} rowToDrag
 * @param {XulToolbarItem} rowToDropAfter
 * @param {Window} win
 */
async function dropAfter(rowToDrag, rowToDropAfter, win) {
  const rect = rowToDropAfter.getBoundingClientRect();
  await drop(
    rowToDrag,
    rowToDropAfter,
    rect.left + 1,
    rect.top + 0.75 * rect.height,
    win
  );
}

/**
 * Virtually drag and drop one tabs list row before another.
 * @param {XulToolbarItem} rowToDrag
 * @param {XulToolbarItem} rowToDropBefore
 * @param {Window} win
 */
async function dropBefore(rowToDrag, rowToDropBefore, win) {
  const rect = rowToDropBefore.getBoundingClientRect();
  await drop(
    rowToDrag,
    rowToDropBefore,
    rect.left + 1,
    rect.top + 0.25 * rect.height,
    win
  );
}

/**
 * @param {XulToolbarItem} row
 * @param {MozTabbrowserTabGroup} tabGroup
 */
function assertTabGroupLabel(row, tabGroup) {
  const rowId = getRowId(row);
  Assert.equal(tabGroupOf(row), tabGroup, `tab group ${rowId} label`);
}

/**
 * @param {XulToolbarItem} row
 */
function assertUngroupedTab(row) {
  const rowId = getRowId(row);
  Assert.ok(!tabOf(row).group, `tab ${rowId} is not in a tab group`);
}

/**
 * @param {XulToolbarItem} row
 * @param {MozTabbrowserTabGroup} tabGroup
 */
function assertGroupedTab(row, tabGroup) {
  const tabRowId = getRowId(row);
  const tabGroupId = getTabGroupIdFromTabGroup(tabGroup);
  Assert.equal(
    tabOf(row).group,
    tabGroup,
    `tab ${tabRowId} is in tab group ${tabGroupId}`
  );
}

add_task(async function test_reorder() {
  await testWithNewWindow(async function (newWindow) {
    Services.telemetry.clearScalars();

    const tabsListNode = newWindow.document.getElementById(
      "allTabsMenu-allTabsView-tabs"
    );

    assertOrder(
      getTabsListOrderedIds(tabsListNode),
      ["unknown", 1, 2, 3, 4, 5],
      "before reorder"
    );

    let rows;
    rows = tabsListNode.querySelectorAll("toolbaritem");
    await dropBefore(rows[3], rows[1], newWindow);

    assertOrder(
      getTabsListOrderedIds(tabsListNode),
      ["unknown", 3, 1, 2, 4, 5],
      "after moving up"
    );

    rows = tabsListNode.querySelectorAll("toolbaritem");
    await dropAfter(rows[1], rows[4], newWindow);

    assertOrder(
      getTabsListOrderedIds(tabsListNode),
      ["unknown", 1, 2, 4, 3, 5],
      "after moving down"
    );

    rows = tabsListNode.querySelectorAll("toolbaritem");
    await dropBefore(rows[4], rows[3], newWindow);

    assertOrder(
      getTabsListOrderedIds(tabsListNode),
      ["unknown", 1, 2, 3, 4, 5],
      "after moving up again"
    );

    let scalars = TelemetryTestUtils.getProcessScalars("parent", false, true);
    TelemetryTestUtils.assertScalar(
      scalars,
      "browser.ui.interaction.all_tabs_panel_dragstart_tab_event_count",
      3
    );
  });
});

add_task(async function test_move_to_tab_bar() {
  await testWithNewWindow(async function (newWindow) {
    Services.telemetry.clearScalars();

    const tabsListNode = newWindow.document.getElementById(
      "allTabsMenu-allTabsView-tabs"
    );

    assertOrder(
      getTabsListOrderedIds(tabsListNode),
      ["unknown", 1, 2, 3, 4, 5],
      "before reorder"
    );

    let rows;
    rows = tabsListNode.querySelectorAll("toolbaritem");
    EventUtils.synthesizeDrop(
      rows[3],
      tabOf(rows[1]),
      null,
      "move",
      newWindow,
      newWindow,
      { clientX: 0, clientY: 0 }
    );

    assertOrder(
      getTabsListOrderedIds(tabsListNode),
      ["unknown", 3, 1, 2, 4, 5],
      "after moving up with tab bar"
    );

    rows = tabsListNode.querySelectorAll("toolbaritem");
    EventUtils.synthesizeDrop(
      rows[1],
      tabOf(rows[4]),
      null,
      "move",
      newWindow,
      newWindow,
      { clientX: 0, clientY: 0 }
    );
    await newWindow.gTabsPanel.allTabsPanel.domRefreshComplete;

    assertOrder(
      getTabsListOrderedIds(tabsListNode),
      ["unknown", 1, 2, 3, 4, 5],
      "after moving down with tab bar"
    );

    let scalars = TelemetryTestUtils.getProcessScalars("parent", false, true);
    TelemetryTestUtils.assertScalar(
      scalars,
      "browser.ui.interaction.all_tabs_panel_dragstart_tab_event_count",
      2
    );
  });
});

add_task(async function test_move_to_different_tab_bar() {
  const newWindow2 = await BrowserTestUtils.openNewBrowserWindow();

  await testWithNewWindow(async function (newWindow) {
    Services.telemetry.clearScalars();

    const tabsListNode = newWindow.document.getElementById(
      "allTabsMenu-allTabsView-tabs"
    );

    assertOrder(
      getTabsListOrderedIds(tabsListNode),
      ["unknown", 1, 2, 3, 4, 5],
      "before reorder in newWindow"
    );
    assertOrder(
      getTabStripOrderedIds(newWindow2),
      ["unknown"],
      "before reorder in newWindow2"
    );

    let rows;
    rows = tabsListNode.querySelectorAll("toolbaritem");
    EventUtils.synthesizeDrop(
      rows[3],
      newWindow2.gBrowser.tabs[0],
      null,
      "move",
      newWindow,
      newWindow2,
      { clientX: 0, clientY: 0 }
    );
    await newWindow.gTabsPanel.allTabsPanel.domRefreshComplete;

    assertOrder(
      getTabsListOrderedIds(tabsListNode),
      ["unknown", 1, 2, 4, 5],
      "after moving to other window in newWindow"
    );

    assertOrder(
      getTabStripOrderedIds(newWindow2),
      [3, "unknown"],
      "after moving to other window in newWindow2"
    );

    let scalars = TelemetryTestUtils.getProcessScalars("parent", false, true);
    TelemetryTestUtils.assertScalar(
      scalars,
      "browser.ui.interaction.all_tabs_panel_dragstart_tab_event_count",
      1
    );
  });

  await BrowserTestUtils.closeWindow(newWindow2);
});

add_task(async function test_drag_and_drop_to_bookmark_toolbar() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.toolbars.bookmarks.visibility", "always"]],
  });

  await PlacesUtils.bookmarks.eraseEverything();
  registerCleanupFunction(async () => {
    await PlacesUtils.bookmarks.eraseEverything();
  });

  await testWithNewWindow(async function (newWindow) {
    is(
      await PlacesUtils.bookmarks.fetch({
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: 0,
      }),
      null,
      "The bookmark toolbar shouldn't have any item"
    );

    const bookmarkToolbar =
      newWindow.document.getElementById("PlacesToolbarItems");

    // Wait if the bookmark toolbar initialization hasn't finished.
    await BrowserTestUtils.waitForMutationCondition(
      bookmarkToolbar,
      { attributes: true, childNodes: true, attributeFilter: ["collapsed"] },
      () => !bookmarkToolbar.collapsed && !bookmarkToolbar.childNodes.length
    );

    newWindow.gBrowser.removeTab(newWindow.gBrowser.selectedTab);

    const tabsListNode = newWindow.document.getElementById(
      "allTabsMenu-allTabsView-tabs"
    );

    assertOrder(
      getTabsListOrderedIds(tabsListNode),
      [1, 2, 3, 4, 5],
      "0-th tab is closed"
    );

    is(getTabIdFromTab(newWindow.gBrowser.selectedTab), 1, "1th tab is active");

    const { PlacesTestUtils } = ChromeUtils.importESModule(
      "resource://testing-common/PlacesTestUtils.sys.mjs"
    );

    const bookmarkPromise = PlacesTestUtils.waitForNotification(
      "bookmark-added",
      events => events.some(e => e.url == URL5)
    );

    // Drag and drop the 5st tab to the bookmark toolbar, while the active tab
    // is the 1th tab.
    const rows = tabsListNode.querySelectorAll("toolbaritem");
    EventUtils.synthesizeDrop(
      rows[4],
      bookmarkToolbar,
      null,
      "move",
      newWindow,
      newWindow,
      { clientX: 0, clientY: 0 }
    );

    await bookmarkPromise;

    is(
      (
        await PlacesUtils.bookmarks.fetch({
          parentGuid: PlacesUtils.bookmarks.toolbarGuid,
          index: 0,
        })
      ).url.href,
      URL5,
      "5th tab should be bookmarked"
    );

    is(
      await PlacesUtils.bookmarks.fetch({
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        index: 1,
      }),
      null,
      "No other tabs should be bookmarked"
    );
  });

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_drag_and_drop_tab_groups() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.groups.enabled", true]],
  });

  await testWithNewWindow(async function (newWindow) {
    const tabsListNode = newWindow.gTabsPanel.allTabsPanel.containerNode;

    assertOrder(
      getTabsListOrderedIds(tabsListNode),
      ["unknown", 1, 2, 3, 4, 5],
      "before creating any tab groups"
    );

    const tab2 = newWindow.gBrowser.tabs.at(2);
    const tab4 = newWindow.gBrowser.tabs.at(4);

    const tabGroup1 = newWindow.gBrowser.addTabGroup([tab2], {
      id: TAB_GROUP_1,
      insertBefore: tab2,
    });

    const tabGroup2 = newWindow.gBrowser.addTabGroup([tab4], {
      id: TAB_GROUP_2,
      insertBefore: tab4,
    });

    await newWindow.gTabsPanel.allTabsPanel.domRefreshComplete;

    assertOrder(
      getTabsListOrderedIds(tabsListNode),
      ["unknown", 1, TAB_GROUP_1, 2, 3, TAB_GROUP_2, 4, 5],
      "after creating the initial tab groups"
    );

    info("drag a whole tab group to the start of the tab list");
    let rows = tabsListNode.querySelectorAll("toolbaritem");
    await dropBefore(
      rows[2], // TAB_GROUP_1 tab group label
      rows[0], // initial "unknown" tab from the test window
      newWindow
    );
    assertOrder(
      getTabsListOrderedIds(tabsListNode),
      [TAB_GROUP_1, 2, "unknown", 1, 3, TAB_GROUP_2, 4, 5],
      "after dragging tab group 1 to the start of the tab menu"
    );

    info("drag a whole tab group to the end of the tab list");
    rows = tabsListNode.querySelectorAll("toolbaritem");
    await dropAfter(
      rows[5], // TAB_GROUP_2 tab group label
      rows[7], // tab 5
      newWindow
    );
    assertOrder(
      getTabsListOrderedIds(tabsListNode),
      [TAB_GROUP_1, 2, "unknown", 1, 3, 5, TAB_GROUP_2, 4],
      "after dragging tab group 2 to the end of the tab menu"
    );

    info(
      "drag a tab into the beginning of a tab group: drop after group label"
    );
    rows = tabsListNode.querySelectorAll("toolbaritem");
    await dropAfter(
      rows[4], // tab 3
      rows[0], // TAB_GROUP_1 tab group label
      newWindow
    );
    assertOrder(
      getTabsListOrderedIds(tabsListNode),
      [TAB_GROUP_1, 3, 2, "unknown", 1, 5, TAB_GROUP_2, 4],
      "after dragging tab 3 to the start of tab group 1"
    );
    rows = tabsListNode.querySelectorAll("toolbaritem");
    assertGroupedTab(rows[1], tabGroup1);

    info(
      "drag a tab into the beginning of a tab group: drop before first grouped tab"
    );
    rows = tabsListNode.querySelectorAll("toolbaritem");
    await dropBefore(
      rows[5], // tab 5
      rows[1], // tab 3
      newWindow
    );
    assertOrder(
      getTabsListOrderedIds(tabsListNode),
      [TAB_GROUP_1, 5, 3, 2, "unknown", 1, TAB_GROUP_2, 4],
      "after dragging tab 5 to the start of tab group 1"
    );
    rows = tabsListNode.querySelectorAll("toolbaritem");
    assertGroupedTab(rows[1], tabGroup1);

    info("try to drag a whole tab group above a tab in another group");
    rows = tabsListNode.querySelectorAll("toolbaritem");
    await dropBefore(
      rows[6], // TAB_GROUP_2 tab group label
      rows[2], // tab 3
      newWindow
    );
    assertOrder(
      getTabsListOrderedIds(tabsListNode),
      [TAB_GROUP_2, 4, TAB_GROUP_1, 5, 3, 2, "unknown", 1],
      "after dragging tab group 2 above a tab inside of tab group 1, tab group 2 should be before tab group 1"
    );

    info("try to drag a whole tab group below a tab in another group");
    rows = tabsListNode.querySelectorAll("toolbaritem");
    await dropAfter(
      rows[0], // TAB_GROUP_2 tab group label
      rows[3], // tab 5
      newWindow
    );
    assertOrder(
      getTabsListOrderedIds(tabsListNode),
      [TAB_GROUP_1, 5, 3, 2, TAB_GROUP_2, 4, "unknown", 1],
      "after dragging tab group 2 below a tab inside of tab group 1, tab group 2 should be after tab group 1"
    );

    info("drag a grouped tab next to an ungrouped tab");
    rows = tabsListNode.querySelectorAll("toolbaritem");
    await dropAfter(
      rows[1], // tab 5
      rows[7], // tab 1
      newWindow
    );
    assertOrder(
      getTabsListOrderedIds(tabsListNode),
      [TAB_GROUP_1, 3, 2, TAB_GROUP_2, 4, "unknown", 1, 5],
      "after dragging tab 5 below ungrouped tab 1, tab 5 should be the last tab"
    );
    rows = tabsListNode.querySelectorAll("toolbaritem");
    assertUngroupedTab(rows[7]);

    info("validate tab group membership for all menu items");
    rows = tabsListNode.querySelectorAll("toolbaritem");
    assertTabGroupLabel(rows[0], tabGroup1);
    assertGroupedTab(rows[1], tabGroup1);
    assertGroupedTab(rows[2], tabGroup1);
    assertTabGroupLabel(rows[3], tabGroup2);
    assertGroupedTab(rows[4], tabGroup2);
    assertUngroupedTab(rows[5]);
    assertUngroupedTab(rows[6]);
    assertUngroupedTab(rows[7]);
  });

  await SpecialPowers.popPrefEnv();
});
