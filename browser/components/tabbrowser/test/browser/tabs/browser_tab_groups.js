/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { ASRouterTriggerListeners } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/ASRouterTriggerListeners.sys.mjs"
);
const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
const { TabStateFlusher } = ChromeUtils.importESModule(
  "resource:///modules/sessionstore/TabStateFlusher.sys.mjs"
);

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.groups.enabled", true]],
  });
});

function createManyTabs(number, win = window) {
  return Array.from({ length: number }, () => {
    return BrowserTestUtils.addTab(win.gBrowser, "about:blank", {
      skipAnimation: true,
    });
  });
}

async function waitForAndAcceptGroupPanel(actionCallback) {
  let editor = document.getElementById("tab-group-editor");
  let panelShown = BrowserTestUtils.waitForPopupEvent(editor.panel, "shown");
  let done = BrowserTestUtils.waitForEvent(editor, "TabGroupCreateDone");
  await actionCallback();
  await panelShown;
  EventUtils.synthesizeKey("VK_RETURN");
  await done;
}

add_task(async function test_tabGroupCreateAndAddTab() {
  let tab1 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let group = gBrowser.addTabGroup([tab1]);

  Assert.ok(group.id, "group has id");
  Assert.ok(group.tabs.includes(tab1), "tab1 is in group");

  let tab2 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  group.addTabs([tab2]);

  Assert.equal(group.tabs.length, 2, "group has 2 tabs");
  Assert.ok(group.tabs.includes(tab2), "tab2 is in group");

  await removeTabGroup(group);
});

add_task(async function test_tabGroupCreateAndAddTabAtPosition() {
  let tabs = createManyTabs(10);
  let tabToGroup = tabs[5];
  let originalPos = tabToGroup._tPos;
  gBrowser.addTabGroup([tabs[5]], { insertBefore: tabs[5] });

  Assert.equal(tabToGroup._tPos, originalPos, "tab has not changed position");

  tabs.forEach(t => {
    BrowserTestUtils.removeTab(t);
  });
});

add_task(async function test_pinned() {
  let tab1 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab2 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  gBrowser.pinTab(tab1);
  gBrowser.pinTab(tab2);
  Assert.ok(tab1.pinned, "Tab1 is pinned");
  Assert.ok(tab2.pinned, "Tab2 is pinned");
  let group = gBrowser.addTabGroup([tab1, tab2]);
  Assert.equal(
    group.tabs.length,
    2,
    "addTabGroup creates a group when only supplied with pinned tabs"
  );
  Assert.ok(!tab1.pinned, "Tab1 is no longer pinned");
  Assert.ok(!tab2.pinned, "Tab2 is no longer pinned");
  group.ungroupTabs();
  gBrowser.pinTab(tab1);
  Assert.ok(tab1.pinned, "Tab1 is pinned again");

  group = gBrowser.addTabGroup([tab1, tab2]);
  Assert.ok(
    group,
    "addTabGroup should create a group when supplied with both pinned and non-pinned tabs"
  );

  Assert.equal(group.tabs.length, 2, "group contains both tabs");
  Assert.ok(!tab1.pinned, "tab1 is no longer pinned");

  BrowserTestUtils.removeTab(tab1);
  await removeTabGroup(group);
});

add_task(async function test_getTabGroups() {
  let tab1 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let group1 = gBrowser.addTabGroup([tab1]);
  Assert.equal(
    gBrowser.tabGroups.length,
    1,
    "there is one group in the tabstrip"
  );

  let tab2 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let group2 = gBrowser.addTabGroup([tab2]);
  Assert.equal(
    gBrowser.tabGroups.length,
    2,
    "there are two groups in the tabstrip"
  );

  await removeTabGroup(group1);
  await removeTabGroup(group2);
  Assert.equal(
    gBrowser.tabGroups.length,
    0,
    "there are no groups in the tabstrip"
  );
});

/**
 * Tests that creating a group without specifying a color will select a
 * unique color.
 */
add_task(async function test_tabGroupUniqueColors() {
  let initialTab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    skipAnimation: true,
  });
  let initialGroup = gBrowser.addTabGroup([initialTab]);
  let existingGroups = [initialGroup];

  for (let i = 2; i <= 9; i++) {
    let newTab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
      skipAnimation: true,
    });
    let newGroup = gBrowser.addTabGroup([newTab]);
    Assert.ok(
      !existingGroups.find(grp => grp.color == newGroup.color),
      `Group ${i} has a distinct color`
    );
    existingGroups.push(newGroup);
  }

  for (let group of existingGroups) {
    await removeTabGroup(group);
  }
});

add_task(async function test_tabGroupCollapseAndExpand() {
  let tab1 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab2 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let group = gBrowser.addTabGroup([tab1]);

  Assert.ok(!group.collapsed, "group is expanded by default");

  group.querySelector(".tab-group-label").click();
  Assert.ok(group.collapsed, "group is collapsed on click");

  group.querySelector(".tab-group-label").click();
  Assert.ok(!group.collapsed, "collapsed group is expanded on click");

  group.collapsed = true;
  Assert.ok(group.collapsed, "group is collapsed via API");
  gBrowser.selectedTab = group.tabs[0];
  Assert.ok(!group.collapsed, "group is expanded after selecting tab");

  group.collapsed = true;
  Assert.ok(group.collapsed, "group is collapsed via API");
  gBrowser.moveTabToGroup(tab2, group);
  Assert.ok(!group.collapsed, "group is expanded after moving tab into group");

  await removeTabGroup(group);
});

add_task(async function test_tabGroupCollapsedTabsNotVisible() {
  let tab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let group = gBrowser.addTabGroup([tab]);

  Assert.ok(!group.collapsed, "group is expanded by default");

  Assert.ok(
    gBrowser.visibleTabs.includes(tab),
    "tab in expanded tab group is visible"
  );

  group.collapsed = true;
  Assert.ok(
    !gBrowser.visibleTabs.includes(tab),
    "tab in collapsed tab group is not visible"
  );

  // TODO gBrowser.removeTabs breaks if the tab is not in a visible state
  group.collapsed = false;
  await removeTabGroup(group);
});

/*
 * Tests that if a tab group is collapsed while the selected tab is in the group,
 * the selected tab will change to be the adjacent tab just after the group.
 *
 * This tests that the tab after the group will be prioritized over the tab
 * just before the group, if both exist.
 */
add_task(async function test_tabGroupCollapseSelectsAdjacentTabAfter() {
  let tabInGroup = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let group = gBrowser.addTabGroup([tabInGroup]);
  let adjacentTabAfter = BrowserTestUtils.addTab(gBrowser, "about:blank");

  gBrowser.selectedTab = tabInGroup;

  group.collapsed = true;
  Assert.equal(
    gBrowser.selectedTab,
    adjacentTabAfter,
    "selected tab becomes adjacent tab after group on collapse"
  );

  BrowserTestUtils.removeTab(adjacentTabAfter);
  // TODO gBrowser.removeTabs breaks if the tab is not in a visible state
  group.collapsed = false;
  await removeTabGroup(group);
});

/*
 * Tests that if a tab group is collapsed while the selected tab is in the group,
 * the selected tab will change to be the adjacent tab just before the group,
 * if no tabs exist after the group
 */
add_task(async function test_tabGroupCollapseSelectsAdjacentTabBefore() {
  let adjacentTabBefore = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tabInGroup = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let group = gBrowser.addTabGroup([tabInGroup]);

  gBrowser.selectedTab = tabInGroup;

  group.collapsed = true;
  Assert.equal(
    gBrowser.selectedTab,
    adjacentTabBefore,
    "selected tab becomes adjacent tab after group on collapse"
  );

  BrowserTestUtils.removeTab(adjacentTabBefore);
  group.collapsed = false;
  await removeTabGroup(group);
});

add_task(async function test_tabGroupCollapseCreatesNewTabIfAllTabsInGroup() {
  // This test has to be run in a new window because there is currently no
  // API to remove a tab from a group, which breaks tests following this one
  // This can be removed once the group remove API is implemented
  let fgWindow = await BrowserTestUtils.openNewBrowserWindow();

  let group = fgWindow.gBrowser.addTabGroup(fgWindow.gBrowser.tabs);

  Assert.equal(fgWindow.gBrowser.tabs.length, 1, "only one tab exists");
  Assert.equal(
    fgWindow.gBrowser.tabs[0].group,
    group,
    "sole existing tab is in group"
  );

  group.collapsed = true;

  Assert.equal(
    fgWindow.gBrowser.tabs.length,
    2,
    "new tab is created if group is collapsed and all tabs are in group"
  );
  Assert.equal(
    fgWindow.gBrowser.selectedTab,
    fgWindow.gBrowser.tabs[1],
    "new tab becomes selected tab"
  );
  Assert.equal(
    fgWindow.gBrowser.selectedTab.group,
    null,
    "new tab is not in group"
  );

  // TODO gBrowser.removeTabs breaks if the tab is not in a visible state
  group.collapsed = false;
  await removeTabGroup(group);
  await BrowserTestUtils.closeWindow(fgWindow);
});

add_task(async function test_collapseAllGroups() {
  // When collapsing a group and no tabs exist outside of collapsed groups, a
  // new tab should be opened.
  let fgWindow = await BrowserTestUtils.openNewBrowserWindow();

  Assert.equal(fgWindow.gBrowser.tabs.length, 1, "only one tab exists");
  let [tab1] = fgWindow.gBrowser.tabs;
  let tab2 = BrowserTestUtils.addTab(fgWindow.gBrowser, "about:blank", {
    skipAnimation: true,
  });
  let group1 = fgWindow.gBrowser.addTabGroup([tab1]);
  let group2 = fgWindow.gBrowser.addTabGroup([tab2]);

  Assert.ok(tab1.selected, "tab1 is selected initially");
  group1.collapsed = true;
  Assert.ok(tab2.selected, "tab2 is selected after collapsing group1");

  let newTabPromise = BrowserTestUtils.waitForEvent(fgWindow, "TabOpen");
  group2.collapsed = true;
  info("Waiting for new tab to open");
  let { target: newTab } = await newTabPromise;
  Assert.ok(group2.collapsed, "successfully collapsed group2");
  Assert.ok(group1.collapsed, "group1 is still collapsed");
  Assert.ok(
    newTab.selected,
    "opened a new tab and selected it after collapsing group2"
  );

  await BrowserTestUtils.closeWindow(fgWindow);
});

add_task(async function test_closingLastTabBeforeCollapsedTabGroup() {
  // If there is one standalone tab that's active and there is a collapsed
  // tab group, and the user closes the standalone tab, the first tab of
  // the collapsed tab group should become the active tab (also expanding
  // the tab group in the process)
  let fgWindow = await BrowserTestUtils.openNewBrowserWindow();

  Assert.equal(fgWindow.gBrowser.tabs.length, 1, "only one tab exists");
  let [standaloneTab] = fgWindow.gBrowser.tabs;

  let groupedTab1 = BrowserTestUtils.addTab(fgWindow.gBrowser, "about:blank", {
    skipAnimation: true,
  });
  let groupedTab2 = BrowserTestUtils.addTab(fgWindow.gBrowser, "about:blank", {
    skipAnimation: true,
  });
  let group = fgWindow.gBrowser.addTabGroup([groupedTab1, groupedTab2]);
  group.collapsed = true;

  fgWindow.gBrowser.selectedTab = standaloneTab;

  let waitForClose = BrowserTestUtils.waitForTabClosing(standaloneTab);
  BrowserTestUtils.removeTab(standaloneTab);
  await waitForClose;

  Assert.equal(
    fgWindow.gBrowser.selectedTab,
    groupedTab1,
    "first tab in the group should be the active tab"
  );
  Assert.ok(!group.collapsed, "tab group should now be expanded");

  await BrowserTestUtils.closeWindow(fgWindow);
});

add_task(async function test_closingLastTabAfterCollapsedTabGroup() {
  // If there is a collapsed tab group followed by a single standalone tab,
  // and the user closes the standalone tab, the last tab of the collapsed
  // tab group should become the active tab (also expanding the tab group
  // in the process)
  let fgWindow = await BrowserTestUtils.openNewBrowserWindow();

  Assert.equal(fgWindow.gBrowser.tabs.length, 1, "only one tab exists");
  let [standaloneTab] = fgWindow.gBrowser.tabs;

  let groupedTab1 = BrowserTestUtils.addTab(fgWindow.gBrowser, "about:blank", {
    skipAnimation: true,
  });
  let groupedTab2 = BrowserTestUtils.addTab(fgWindow.gBrowser, "about:blank", {
    skipAnimation: true,
  });
  let group = fgWindow.gBrowser.addTabGroup([groupedTab1, groupedTab2], {
    insertBefore: standaloneTab,
  });
  group.collapsed = true;

  fgWindow.gBrowser.selectedTab = standaloneTab;

  let waitForClose = BrowserTestUtils.waitForTabClosing(standaloneTab);
  BrowserTestUtils.removeTab(standaloneTab);
  await waitForClose;

  Assert.equal(
    fgWindow.gBrowser.selectedTab,
    groupedTab2,
    "last tab in the group should be the active tab"
  );
  Assert.ok(!group.collapsed, "tab group should now be expanded");

  await BrowserTestUtils.closeWindow(fgWindow);
});

add_task(async function test_tabUngroup() {
  let extraTab1 = BrowserTestUtils.addTab(gBrowser, "about:blank");

  let groupedTab1 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let groupedTab2 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let group = gBrowser.addTabGroup([groupedTab1, groupedTab2]);

  let extraTab2 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let group2 = gBrowser.addTabGroup([extraTab2]);

  Assert.equal(
    groupedTab1._tPos,
    2,
    "grouped tab 1 starts in correct position"
  );
  Assert.equal(
    groupedTab2._tPos,
    3,
    "grouped tab 2 starts in correct position"
  );
  Assert.equal(groupedTab1.group, group, "tab 1 belongs to group");
  Assert.equal(groupedTab2.group, group, "tab 2 belongs to group");

  info("Calling ungroupTabs and waiting for TabGroupRemoved event.");
  let removePromise = BrowserTestUtils.waitForEvent(group, "TabGroupRemoved");
  group.ungroupTabs();
  await removePromise;

  Assert.equal(
    groupedTab1._tPos,
    2,
    "tab 1 is in the same position as before ungroup"
  );
  Assert.equal(
    groupedTab2._tPos,
    3,
    "tab 2 is in the same position as before ungroup"
  );
  Assert.equal(groupedTab1.group, null, "tab 1 no longer belongs to group");
  Assert.equal(groupedTab2.group, null, "tab 2 no longer belongs to group");
  Assert.equal(
    groupedTab1.nextElementSibling,
    groupedTab2,
    "tab 1 moved before tab 2"
  );
  Assert.equal(
    groupedTab2.nextElementSibling,
    group2,
    "tab 2 moved before the next group"
  );

  BrowserTestUtils.removeTab(groupedTab1);
  BrowserTestUtils.removeTab(groupedTab2);
  BrowserTestUtils.removeTab(extraTab1);
  BrowserTestUtils.removeTab(extraTab2);
});

add_task(async function test_tabGroupRemove() {
  let groupedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let group = gBrowser.addTabGroup([groupedTab]);

  await removeTabGroup(group);

  Assert.equal(groupedTab.parentElement, null, "grouped tab is unloaded");
  Assert.equal(group.parentElement, null, "group is unloaded");
});

add_task(async function test_tabGroupDeletesWhenLastTabClosed() {
  let tab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let group = gBrowser.addTabGroup([tab]);

  gBrowser.removeTab(tab);

  Assert.equal(group.parent, null, "group is removed from tabbrowser");
});

add_task(async function test_tabGroupMoveToNewWindow() {
  let tabUri = "https://example.com/tab-group-test";
  let groupedTab = await addTab(tabUri);
  let group = gBrowser.addTabGroup([groupedTab], {
    color: "blue",
    label: "test",
  });

  info("Calling adoptTabGroup and waiting for TabGroupRemoved event.");
  let removePromise = BrowserTestUtils.waitForEvent(group, "TabGroupRemoved");
  let fgWindow = await BrowserTestUtils.openNewBrowserWindow();
  let tabGroupCreate = BrowserTestUtils.waitForEvent(
    fgWindow,
    "TabGroupCreate"
  );
  fgWindow.gBrowser.adoptTabGroup(group, { elementIndex: 0 });
  let [, tabGroupCreateEvent] = await Promise.all([
    removePromise,
    tabGroupCreate,
  ]);
  Assert.ok(
    tabGroupCreateEvent.detail.isAdoptingGroup,
    "TabGroupCreate event should report that this tab group was creating by adoption"
  );

  Assert.equal(
    gBrowser.tabGroups.length,
    0,
    "Tab group no longer exists in original window"
  );
  Assert.equal(
    fgWindow.gBrowser.tabGroups.length,
    1,
    "A tab group exists in the new window"
  );

  let newGroup = fgWindow.gBrowser.tabGroups[0];

  Assert.equal(
    newGroup.color,
    "blue",
    "New group has same color as original group"
  );
  Assert.equal(
    newGroup.label,
    "test",
    "New group has same label as original group"
  );
  Assert.equal(
    newGroup.tabs.length,
    1,
    "New group has same number of tabs as original group"
  );
  Assert.equal(
    newGroup.tabs[0].linkedBrowser.currentURI.spec,
    tabUri,
    "New tab has same URI as old tab"
  );

  await removeTabGroup(newGroup);
  await BrowserTestUtils.closeWindow(fgWindow);
});

add_task(async function test_TabGroupEvents() {
  const triggerHandler = sinon.stub();
  const tabGroupCollapsedTrigger =
    ASRouterTriggerListeners.get("tabGroupCollapsed");
  tabGroupCollapsedTrigger.uninit();
  tabGroupCollapsedTrigger.init(triggerHandler);

  let tab1 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab2 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let group;

  let createdGroupId = null;
  let tabGroupCreated = BrowserTestUtils.waitForEvent(
    window,
    "TabGroupCreate"
  ).then(event => {
    Assert.ok(
      !event.detail.isAdoptingGroup,
      "a tab group being created from scratch should not be treated like it was adopted from another window"
    );
    createdGroupId = event.target.id;
  });
  group = gBrowser.addTabGroup([tab1]);
  await tabGroupCreated;
  Assert.equal(
    createdGroupId,
    group.id,
    "TabGroupCreate fired with correct group as target"
  );

  let groupedGroupId = null;
  let groupedTab = null;
  let tabGrouped = BrowserTestUtils.waitForEvent(group, "TabGrouped").then(
    event => {
      groupedGroupId = event.target.id;
      groupedTab = event.detail;
    }
  );
  group.addTabs([tab2]);
  await tabGrouped;
  Assert.equal(groupedGroupId, group.id, "TabGrouped fired with correct group");
  Assert.equal(groupedTab, tab2, "TabGrouped fired with correct tab");

  let groupCollapsed = BrowserTestUtils.waitForEvent(group, "TabGroupCollapse");
  group.collapsed = true;
  await groupCollapsed;
  Assert.ok(triggerHandler.calledOnce, "Called once after tab group collapsed");

  let groupExpanded = BrowserTestUtils.waitForEvent(group, "TabGroupExpand");
  group.collapsed = false;
  await groupExpanded;

  let ungroupedGroupId = null;
  let ungroupedTab = null;
  let tabUngrouped = BrowserTestUtils.waitForEvent(group, "TabUngrouped").then(
    event => {
      ungroupedGroupId = event.target.id;
      ungroupedTab = event.detail;
    }
  );
  gBrowser.moveTabToStart(tab2);
  await tabUngrouped;
  Assert.equal(
    ungroupedGroupId,
    group.id,
    "TabUngrouped fired with correct group"
  );
  Assert.equal(ungroupedTab, tab2, "TabUngrouped fired with correct tab");

  let tabGroupRemoved = BrowserTestUtils.waitForEvent(group, "TabGroupRemoved");
  await removeTabGroup(group);
  await tabGroupRemoved;

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  tabGroupCollapsedTrigger.uninit();
});

add_task(async function test_moveTabGroup() {
  let tab1 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab2 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let group = gBrowser.addTabGroup([tab1, tab2]);

  let tabMoveEvents = Promise.all([
    BrowserTestUtils.waitForEvent(tab1, "TabMove"),
    BrowserTestUtils.waitForEvent(tab2, "TabMove"),
  ]);
  info("moving tab group and awaiting TabMove events");
  gBrowser.moveTabToStart(group);
  await tabMoveEvents;

  await removeTabGroup(group);
});

add_task(async function test_moveTabBetweenGroups() {
  let tab1 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab2 = BrowserTestUtils.addTab(gBrowser, "about:blank");

  let tab1Added = BrowserTestUtils.waitForEvent(window, "TabGrouped");
  let tab2Added = BrowserTestUtils.waitForEvent(window, "TabGrouped");
  let group1 = gBrowser.addTabGroup([tab1]);
  let group2 = gBrowser.addTabGroup([tab2]);
  await Promise.allSettled([tab1Added, tab2Added]);

  let ungroupedGroupId = null;
  let ungroupedTab = null;
  let tabUngrouped = BrowserTestUtils.waitForEvent(window, "TabUngrouped").then(
    event => {
      ungroupedGroupId = event.target.id;
      ungroupedTab = event.detail;
    }
  );
  let groupedGroupId = null;
  let groupedTab = null;
  let tabGrouped = BrowserTestUtils.waitForEvent(window, "TabGrouped").then(
    event => {
      groupedGroupId = event.target.id;
      groupedTab = event.detail;
      Assert.ok(ungroupedGroupId, "TabUngrouped fires before TabGrouped");
    }
  );

  group2.addTabs([tab1]);
  await Promise.allSettled([tabUngrouped, tabGrouped]);
  Assert.equal(ungroupedGroupId, group1.id, "TabUngrouped fired with group1");
  Assert.equal(ungroupedTab, tab1, "TabUngrouped fired with tab1");
  Assert.equal(groupedGroupId, group2.id, "TabGrouped fired with group2");
  Assert.equal(groupedTab, tab1, "TabUngrouped fired with tab1");

  Assert.ok(
    !group1.parent,
    "group1 has been removed after losing its last tab"
  );
  Assert.equal(group2.tabs.length, 2, "group2 has 2 tabs");

  await removeTabGroup(group2);
});

add_task(async function test_moveTabToStartOrEnd() {
  let tab1 = gBrowser.selectedTab;
  let tab2 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  gBrowser.addTabGroup([tab1, tab2]);
  Assert.equal(tab1._tPos, 0, "tab 1 starts at tab index 0");
  Assert.equal(tab2._tPos, 1, "tab 2 starts at tab index 1");
  Assert.equal(
    tab1.elementIndex,
    1,
    "tab 1 starts at element index 1, after the group label"
  );
  Assert.equal(
    tab2.elementIndex,
    2,
    "tab 2 starts at element index 2, after tab 1"
  );

  gBrowser.moveTabToStart(tab1);
  Assert.ok(
    !tab1.group,
    "first tab is not grouped anymore after moving to start"
  );
  Assert.ok(
    tab2.group,
    "last tab is still grouped after moving first tab to start"
  );
  Assert.equal(
    tab1._tPos,
    0,
    "tab 1 remains at tab index 0 after moving to start"
  );
  Assert.equal(
    tab2._tPos,
    1,
    "tab 2 remains at tab index 1 after tab 1 moved to start"
  );
  Assert.equal(
    tab1.elementIndex,
    0,
    "tab 1 moved to element index 0, before the group label"
  );
  Assert.equal(
    tab2.elementIndex,
    2,
    "tab 2 remains at element index 2, after the group label"
  );

  gBrowser.moveTabToEnd(tab2);
  Assert.ok(!tab2.group, "last tab is not grouped anymore after moving to end");
  Assert.equal(
    tab1._tPos,
    0,
    "tab 1 remains at tab index 0 after tab 2 moved to end"
  );
  Assert.equal(
    tab2._tPos,
    1,
    "tab 2 remains at tab index 1 after moving to end"
  );
  Assert.equal(tab1.elementIndex, 0, "tab 1 remains at element index 0");
  Assert.equal(
    tab2.elementIndex,
    1,
    "tab 2 moved at element index 1 since the group label is gone"
  );

  BrowserTestUtils.removeTab(tab2);
});

add_task(async function test_tabGroupSelect() {
  let tab1 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab2 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab3 = BrowserTestUtils.addTab(gBrowser, "about:blank");

  let tab1Added = BrowserTestUtils.waitForEvent(
    window,
    "TabGrouped",
    false,
    ev => ev.detail == tab1
  );
  let tab2Added = BrowserTestUtils.waitForEvent(
    window,
    "TabGrouped",
    false,
    ev => ev.detail == tab2
  );

  let group = gBrowser.addTabGroup([tab1, tab2]);
  await Promise.allSettled([tab1Added, tab2Added]);
  gBrowser.selectTabAtIndex(tab3._tPos);
  Assert.ok(tab3.selected, "Tab 3 is selected");
  group.select();
  Assert.ok(group.tabs[0].selected, "First tab is selected");
  gBrowser.selectTabAtIndex(group.tabs[1]._tPos);
  Assert.ok(group.tabs[1].selected, "Second tab is selected");
  group.select();
  Assert.ok(group.tabs[1].selected, "Second tab is still selected");
  group.collapsed = true;
  Assert.ok(group.collapsed, "Group is collapsed");
  Assert.ok(tab3.selected, "Tab 3 is selected");
  group.select();
  Assert.ok(!group.collapsed, "Group is no longer collapsed");
  Assert.ok(group.tabs[0].selected, "First tab in group is selected");

  await removeTabGroup(group);
  BrowserTestUtils.removeTab(tab3);
});

// Context menu tests
// ---

/**
 * @param {MozTabbrowserTab} tab
 * @param {function(Element?, Element?, Element?):void} callback
 */
const withTabMenu = async function (tab, callback) {
  const tabContextMenu = document.getElementById("tabContextMenu");
  Assert.equal(
    tabContextMenu.state,
    "closed",
    "context menu is initially closed"
  );
  const contextMenuShown = BrowserTestUtils.waitForPopupEvent(
    tabContextMenu,
    "shown"
  );

  EventUtils.synthesizeMouseAtCenter(
    tab,
    { type: "contextmenu", button: 2 },
    window
  );
  await contextMenuShown;

  const moveTabToNewGroupItem = document.getElementById(
    "context_moveTabToNewGroup"
  );
  const moveTabToGroupItem = document.getElementById("context_moveTabToGroup");
  const ungroupTabItem = document.getElementById("context_ungroupTab");
  await callback(moveTabToNewGroupItem, moveTabToGroupItem, ungroupTabItem);

  tabContextMenu.hidePopup();
};

/**
 * @param {MozTabbrowserTab} tab
 * @param {function(MozTabbrowserTab):void} callback
 */
async function withNewTabFromTabMenu(tab, callback) {
  await withTabMenu(tab, async () => {
    const newTabPromise = BrowserTestUtils.waitForEvent(document, "TabOpen");
    const newTabToRight = document.getElementById("context_openANewTab");
    newTabToRight.click();
    const { target: newTab } = await newTabPromise;
    await callback(newTab);
    BrowserTestUtils.removeTab(newTab);
  });
}

/*
 * Tests that the context menu options do not appear if the tab group pref is
 * disabled
 */
add_task(async function test_tabGroupTabContextMenuWithoutPref() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.groups.enabled", false]],
  });

  let tab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    skipAnimation: true,
  });

  await withTabMenu(
    tab,
    async (moveTabToNewGroupItem, moveTabToGroupItem, ungroupTabItem) => {
      Assert.ok(
        moveTabToNewGroupItem.hidden,
        "moveTabToNewGroupItem is hidden"
      );
      Assert.ok(moveTabToGroupItem.hidden, "moveTabToGroupItem is hidden");
      Assert.ok(ungroupTabItem.hidden, "ungroupTabItem is hidden");
    }
  );

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});

// Context menu tests: "move tab to new group" option
// (i.e. the option that appears in the menu when no other groups exist)
// ---

/*
 * Tests that when no groups exist, if a tab is selected, the "move tab to
 * group" option appears in the context menu, and clicking it moves the tab to
 * a new group
 */
add_task(async function test_tabGroupContextMenuMoveTabToNewGroup() {
  let tab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    skipAnimation: true,
  });

  await waitForAndAcceptGroupPanel(
    async () =>
      await withTabMenu(
        tab,
        async (moveTabToNewGroupItem, moveTabToGroupItem) => {
          Assert.equal(tab.group, null, "tab is not in group");
          Assert.ok(
            !moveTabToNewGroupItem.hidden,
            "moveTabToNewGroupItem is visible"
          );
          Assert.ok(moveTabToGroupItem.hidden, "moveTabToGroupItem is hidden");

          moveTabToNewGroupItem.click();
        }
      )
  );

  Assert.ok(tab.group, "tab is in group");
  Assert.equal(tab.group.label, "", "tab group label is empty");
  Assert.equal(
    gBrowser.selectedTab.group?.id,
    tab.group.id,
    "A tab in the group is selected"
  );

  await removeTabGroup(tab.group);
});

/*
 * Tests that when no groups exist, if multiple tabs are selected and one of
 * the selected tabs has its context menu open, the "move tabs to group" option
 * appears in the context menu, and clicking it moves the tabs to a new group
 */
add_task(async function test_tabGroupContextMenuMoveTabsToNewGroup() {
  const tabs = createManyTabs(3);

  // Click the first tab in our test group to make sure the default tab at the
  // start of the tab strip is deselected
  EventUtils.synthesizeMouseAtCenter(tabs[0], {});

  tabs.forEach(t => {
    EventUtils.synthesizeMouseAtCenter(
      t,
      { ctrlKey: true, metaKey: true },
      window
    );
  });

  let tabToClick = tabs[2];

  await waitForAndAcceptGroupPanel(
    async () =>
      await waitForAndAcceptGroupPanel(
        async () =>
          await withTabMenu(
            tabToClick,
            async (moveTabToNewGroupItem, moveTabToGroupItem) => {
              Assert.ok(
                !moveTabToNewGroupItem.hidden,
                "moveTabToNewGroupItem is visible"
              );
              Assert.ok(
                moveTabToGroupItem.hidden,
                "moveTabToGroupItem is hidden"
              );

              moveTabToNewGroupItem.click();
            }
          )
      )
  );

  let group = tabs[0].group;

  Assert.ok(tabs[0].group, "tab is in group");
  Assert.equal(tabs[0].group.label, "", "tab group label is empty");
  tabs.forEach((t, idx) => {
    Assert.equal(t.group, group, `tabs[${idx}] is in group`);
  });

  await removeTabGroup(group);
});

/*
 * Tests that when no groups exist, if a tab is selected and a tab that is
 * *not* selected has its context menu open, the "move tab to group" option
 * appears in the context menu, and clicking it moves the *context menu* tab to
 * the group, not the selected tab
 */
add_task(
  async function test_tabGroupContextMenuMoveTabToNewGroupWhileAnotherSelected() {
    let tab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
      skipAnimation: true,
    });
    let otherTab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
      skipAnimation: true,
    });

    EventUtils.synthesizeMouseAtCenter(otherTab, {});

    await waitForAndAcceptGroupPanel(
      async () =>
        await withTabMenu(
          tab,
          async (moveTabToNewGroupItem, moveTabToGroupItem) => {
            Assert.equal(
              gBrowser.selectedTabs.includes(TabContextMenu.contextTab),
              false,
              "context menu tab is not selected"
            );
            Assert.ok(
              !moveTabToNewGroupItem.hidden,
              "moveTabToNewGroupItem is visible"
            );
            Assert.ok(
              moveTabToGroupItem.hidden,
              "moveTabToGroupItem is hidden"
            );

            moveTabToNewGroupItem.click();
          }
        )
    );

    Assert.ok(tab.group, "tab is in group");
    Assert.equal(otherTab.group, null, "otherTab is not in group");

    await removeTabGroup(tab.group);
    BrowserTestUtils.removeTab(otherTab);
  }
);

/*
 * Tests that when no groups exist, if multiple tabs are selected and a tab
 * that is *not* selected has its context menu open, the "move tabs to group"
 * option appears in the context menu, and clicking it moves the *context menu*
 * tab to the group, not the selected tabs
 */
add_task(
  async function test_tabGroupContextMenuMoveTabToNewGroupWhileOthersSelected() {
    let tab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
      skipAnimation: true,
    });

    const otherTabs = createManyTabs(3);

    otherTabs.forEach(t => {
      EventUtils.synthesizeMouseAtCenter(
        t,
        { ctrlKey: true, metaKey: true },
        window
      );
    });

    await waitForAndAcceptGroupPanel(
      async () =>
        await withTabMenu(
          tab,
          async (moveTabToNewGroupItem, moveTabToGroupItem) => {
            Assert.equal(
              gBrowser.selectedTabs.includes(TabContextMenu.contextTab),
              false,
              "context menu tab is not selected"
            );
            Assert.ok(
              !moveTabToNewGroupItem.hidden,
              "moveTabToNewGroupItem is visible"
            );
            Assert.ok(
              moveTabToGroupItem.hidden,
              "moveTabToGroupItem is hidden"
            );

            moveTabToNewGroupItem.click();
          }
        )
    );

    Assert.ok(tab.group, "tab is in group");

    otherTabs.forEach((t, idx) => {
      Assert.equal(t.group, null, `otherTab[${idx}] is not in group`);
    });

    await removeTabGroup(tab.group);
    otherTabs.forEach(t => {
      BrowserTestUtils.removeTab(t);
    });
  }
);

// Context menu tests: "move tab to group" option
// (i.e. the option that appears in the menu when other groups already exist)
// ---

/*
 * Tests that when groups exist, the "move tab to group" menu option is visible
 * and is correctly populated with the group list
 */
add_task(async function test_tabGroupContextMenuMoveTabToGroupBasics() {
  let tab1 = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    skipAnimation: true,
  });
  let group1 = gBrowser.addTabGroup([tab1], {
    color: "red",
    label: "Test group with label",
  });

  // Make sure the first and second groups have different lastSeenActive times.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 1));

  let tab2 = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    skipAnimation: true,
  });
  let group2 = gBrowser.addTabGroup([tab2], { color: "blue", label: "" });

  Assert.greater(
    group2.lastSeenActive,
    group1.lastSeenActive,
    "last created group should have higher lastSeenActive time"
  );

  let tabToClick = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    skipAnimation: true,
  });

  await withTabMenu(
    tabToClick,
    async (moveTabToNewGroupItem, moveTabToGroupItem) => {
      Assert.ok(
        moveTabToNewGroupItem.hidden,
        "moveTabToNewGroupItem is hidden"
      );
      Assert.ok(!moveTabToGroupItem.hidden, "moveTabToGroupItem is visible");

      const submenu = moveTabToGroupItem.querySelector(
        "#context_moveTabToGroupPopupMenu"
      ).children;

      // Items 0 and 1 are the "new group" item and a separator respectively
      // Note that groups should appear in order of most recently used to least
      const group2Item = submenu[2];
      Assert.equal(
        group2Item.getAttribute("tab-group-id"),
        group2.id,
        "first group in list is group2"
      );
      Assert.equal(
        group2Item.getAttribute("label"),
        "Unnamed group",
        "group2 menu item has correct label"
      );
      Assert.ok(
        group2Item.style
          .getPropertyValue("--tab-group-color")
          .includes("--tab-group-color-blue"),
        "group2 menu item chicklet has correct color"
      );
      Assert.ok(
        group2Item.style
          .getPropertyValue("--tab-group-color-invert")
          .includes("--tab-group-color-blue-invert"),
        "group2 menu item chicklet has correct inverted color"
      );

      const group1Item = submenu[3];
      Assert.equal(
        group1Item.getAttribute("tab-group-id"),
        group1.id,
        "second group in list is group1"
      );
      Assert.equal(
        group1Item.getAttribute("label"),
        "Test group with label",
        "group1 menu item has correct label"
      );
      Assert.ok(
        group1Item.style
          .getPropertyValue("--tab-group-color")
          .includes("--tab-group-color-red"),
        "group1 menu item chicklet has correct color"
      );
      Assert.ok(
        group1Item.style
          .getPropertyValue("--tab-group-color-invert")
          .includes("--tab-group-color-red-invert"),
        "group1 menu item chicklet has correct inverted color"
      );
    }
  );

  await removeTabGroup(group1);
  await removeTabGroup(group2);
  BrowserTestUtils.removeTab(tabToClick);
});

/*
 * Tests that the "move tab to group > new group" option creates a new group and moves the tab to it
 */
add_task(async function test_tabGroupContextMenuMoveTabToGroupNewGroup() {
  let otherTab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    skipAnimation: true,
  });
  let otherGroup = gBrowser.addTabGroup([otherTab]);

  let tab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    skipAnimation: true,
  });

  await waitForAndAcceptGroupPanel(
    async () =>
      await withTabMenu(tab, async (_, moveTabToGroupItem) => {
        moveTabToGroupItem
          .querySelector("#context_moveTabToGroupNewGroup")
          .click();
      })
  );

  Assert.ok(tab.group, "tab is in group");
  Assert.notEqual(
    tab.group.id,
    otherGroup.id,
    "tab is not in the original group"
  );

  await removeTabGroup(otherGroup);
  await removeTabGroup(tab.group);
});

/**
 * Ensure group is positioned correctly when a pinned tab is grouped
 */
add_task(async function test_tabGroupContextMenuMovePinnedTabToNewGroup() {
  let pinnedTab = await addTab("about:blank");
  let pinnedUngroupedTab = await addTab("about:blank");
  gBrowser.pinTab(pinnedTab);
  gBrowser.pinTab(pinnedUngroupedTab);
  await waitForAndAcceptGroupPanel(
    async () =>
      await withTabMenu(pinnedTab, async (_, moveTabToGroupItem) => {
        moveTabToGroupItem
          .querySelector("#context_moveTabToGroupNewGroup")
          .click();
      })
  );
  Assert.ok(!pinnedTab.pinned, "first pinned tab is no longer pinned");
  Assert.ok(pinnedTab.group, "first pinned tab is grouped");
  Assert.ok(
    pinnedTab._tPos > pinnedUngroupedTab._tPos,
    "pinned tab's group appears after the list of pinned tabs"
  );
  await removeTabGroup(pinnedTab.group);
  BrowserTestUtils.removeTab(pinnedUngroupedTab);
});

/*
 * Tests that the "move tab to group > [group name]" option moves a tab to the selected group
 */
add_task(async function test_tabGroupContextMenuMoveTabToExistingGroup() {
  let otherTab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    skipAnimation: true,
  });
  let group = gBrowser.addTabGroup([otherTab]);

  let tab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    skipAnimation: true,
  });

  await withTabMenu(tab, async (_, moveTabToGroupItem) => {
    moveTabToGroupItem.querySelector(`[tab-group-id="${group.id}"]`).click();
  });

  Assert.ok(tab.group, "tab is in group");
  Assert.equal(tab.group.id, group.id, "tab is in the original group");

  await removeTabGroup(group);
});

/*
 * Same as above, but for groups in different windows
 */
add_task(
  async function test_tabGroupContextMenuMoveTabToExistingGroupInDifferentWindow() {
    let otherWindow = await BrowserTestUtils.openNewBrowserWindow();
    let otherTab = await addTabTo(otherWindow.gBrowser, "about:blank", {
      skipAnimation: true,
    });
    let group = otherWindow.gBrowser.addTabGroup([otherTab]);
    let tab = await addTab("about:blank", {
      skipAnimation: true,
    });

    let windowActivated = BrowserTestUtils.waitForEvent(window, "activate");
    window.focus();
    await windowActivated;
    Assert.equal(
      BrowserWindowTracker.getTopWindow(),
      window,
      "current window is active before moving group to another window"
    );

    let tabGrouped = BrowserTestUtils.waitForEvent(otherWindow, "TabGrouped");
    await withTabMenu(tab, async (_, moveTabToGroupItem) => {
      moveTabToGroupItem.querySelector(`[tab-group-id="${group.id}"]`).click();
    });
    await tabGrouped;
    Assert.equal(group.tabs.length, 2, "group has 2 tabs");
    Assert.equal(
      BrowserWindowTracker.getTopWindow(),
      otherWindow,
      "moving group activates target window"
    );

    await BrowserTestUtils.closeWindow(otherWindow);
  }
);

/*
 * Tests that when groups exist, and the context menu tab has a group,
 * that group does not exist in the context menu list
 */
add_task(
  async function test_tabGroupContextMenuMoveTabToGroupContextMenuTabNotInList() {
    let tab1 = BrowserTestUtils.addTab(gBrowser, "about:blank", {
      skipAnimation: true,
    });
    let group1 = gBrowser.addTabGroup([tab1]);
    let tab2 = BrowserTestUtils.addTab(gBrowser, "about:blank", {
      skipAnimation: true,
    });
    let group2 = gBrowser.addTabGroup([tab2]);

    await withTabMenu(tab2, async (_, moveTabToGroupItem) => {
      const submenu = moveTabToGroupItem.querySelector(
        "#context_moveTabToGroupPopupMenu"
      ).children;

      // Accounting for the existence of the "new group" and menuseparator elements
      Assert.equal(submenu.length, 3, "only one tab group exists in the list");
      Assert.equal(
        submenu[2].getAttribute("tab-group-id"),
        group1.id,
        "tab group in the list is not the context menu tab's group"
      );
    });

    await removeTabGroup(group1);
    await removeTabGroup(group2);
  }
);

/*
 * Tests that when only one group exists, and the context menu tab is in the group,
 * the condensed "move tab to new group" menu item is shown in place of the submenu variant
 */
add_task(
  async function test_tabGroupContextMenuMoveTabToGroupOnlyOneGroupIsSelectedGroup() {
    let tab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
      skipAnimation: true,
    });
    let group = gBrowser.addTabGroup([tab]);

    await withTabMenu(
      tab,
      async (moveTabToNewGroupItem, moveTabToGroupItem) => {
        Assert.ok(
          !moveTabToNewGroupItem.hidden,
          "moveTabToNewGroupItem is visible"
        );
        Assert.ok(moveTabToGroupItem.hidden, "moveTabToGroupItem is hidden");
      }
    );

    await removeTabGroup(group);
  }
);

/*
 * Tests that when many groups exist, if many tabs are selected and the
 * selected tabs belong to different groups or are ungrouped, all tab groups
 * appear in the context menu list
 */
add_task(
  async function test_tabGroupContextMenuManySelectedTabsFromManyGroups() {
    const tabs = createManyTabs(3);

    let group1 = gBrowser.addTabGroup([tabs[0]]);
    let group2 = gBrowser.addTabGroup([tabs[1]]);

    tabs.forEach(tab => {
      EventUtils.synthesizeMouseAtCenter(
        tab,
        { ctrlKey: true, metaKey: true },
        window
      );
    });

    const tabToClick = tabs[2];

    await withTabMenu(tabToClick, async (_, moveTabToGroupItem) => {
      const submenu = moveTabToGroupItem.querySelector(
        "#context_moveTabToGroupPopupMenu"
      ).children;

      const tabGroupIds = Array.from(submenu).map(item =>
        item.getAttribute("tab-group-id")
      );

      Assert.ok(
        tabGroupIds.includes(group1.getAttribute("id")),
        "group1 is in context menu list"
      );
      Assert.ok(
        tabGroupIds.includes(group2.getAttribute("id")),
        "group2 is in context menu list"
      );
    });

    await removeTabGroup(group1);
    await removeTabGroup(group2);
    BrowserTestUtils.removeTab(tabToClick);
  }
);

/*
 * Tests that when many groups exist, if many tabs are selected and all the
 * tabs belong to the same group, that group does not appear in the context
 * menu list
 */
add_task(
  async function test_tabGroupContextMenuManySelectedTabsFromSameGroup() {
    const tabsToSelect = createManyTabs(3);
    let selectedTabGroup = gBrowser.addTabGroup(tabsToSelect);
    let otherTab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
      skipAnimation: true,
    });
    let otherGroup = gBrowser.addTabGroup([otherTab]);

    // Click the first tab in our test group to make sure the default tab at the
    // start of the tab strip is deselected
    // This is broken on tabs within tab groups ...
    EventUtils.synthesizeMouseAtCenter(tabsToSelect[0], {});

    tabsToSelect.forEach(tab => {
      EventUtils.synthesizeMouseAtCenter(
        tab,
        { ctrlKey: true, metaKey: true },
        window
      );
    });

    await withTabMenu(tabsToSelect[2], async (_, moveTabToGroupItem) => {
      const submenu = moveTabToGroupItem.querySelector(
        "#context_moveTabToGroupPopupMenu"
      ).children;

      const tabGroupIds = Array.from(submenu).map(item =>
        item.getAttribute("tab-group-id")
      );

      Assert.ok(
        !tabGroupIds.includes(selectedTabGroup.getAttribute("id")),
        "group with selected tabs is not in context menu list"
      );
    });

    let ungroupedTab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
      skipAnimation: true,
    });
    EventUtils.synthesizeMouseAtCenter(
      ungroupedTab,
      { ctrlKey: true, metaKey: true },
      window
    );
    await withTabMenu(tabsToSelect[2], async (_, moveTabToGroupItem) => {
      const submenu = moveTabToGroupItem.querySelector(
        "#context_moveTabToGroupPopupMenu"
      ).children;

      const tabGroupIds = Array.from(submenu).map(item =>
        item.getAttribute("tab-group-id")
      );

      Assert.ok(
        tabGroupIds.includes(selectedTabGroup.getAttribute("id")),
        "group with selected tabs is in context menu list since one of the selected tabs is ungrouped"
      );
    });

    await removeTabGroup(selectedTabGroup);
    await removeTabGroup(otherGroup);
    BrowserTestUtils.removeTab(ungroupedTab);
  }
);

// Context menu tests: "remove from group" option
// ---

/* Tests that if no groups exist within the selection, the "remove from group"
 * option does not exist
 */
add_task(async function test_removeFromGroupHiddenIfNoGroupInSelection() {
  let unrelatedGroupedTab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    skipAnimation: true,
  });
  let unrelatedGroup = gBrowser.addTabGroup([unrelatedGroupedTab]);

  let tab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    skipAnimation: true,
  });

  await withTabMenu(tab, async (_m1, _m2, ungroupTabItem) => {
    Assert.ok(ungroupTabItem.hidden, "ungroupTabItem is hidden");
  });

  BrowserTestUtils.removeTab(tab);
  await removeTabGroup(unrelatedGroup);
});

/* Tests that if a single tab is selected and that tab is part of a group, the
 * "remove from group" option exists and clicking the item removes the tab from
 * the group
 */
add_task(async function test_removeFromGroupForSingleTab() {
  const tabs = createManyTabs(3);
  let group = gBrowser.addTabGroup(tabs);
  let extraTab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    skipAnimation: true,
  });
  let tabToClick = tabs[1];

  Assert.equal(tabToClick.group, group, "tab is in group");

  await withTabMenu(tabToClick, async (_m1, _m2, ungroupTabItem) => {
    Assert.ok(!ungroupTabItem.hidden, "ungroupTabItem is visible");

    ungroupTabItem.click();
  });

  Assert.ok(!tabToClick.group, "tab is no longer in group");
  Assert.equal(
    gBrowser.tabs[3],
    tabToClick,
    "tab has been moved just outside the group in the tab strip"
  );

  await removeTabGroup(group);
  BrowserTestUtils.removeTab(tabToClick);
  BrowserTestUtils.removeTab(extraTab);
});

/* Tests that if many tabs are selected and at least some of those tabs are
 * part of a group, the "remove from group" option exists and clicking the item
 * removes all tabs from their groups
 */
add_task(async function test_removeFromGroupForMultipleTabs() {
  // initial tab strip: [group1, group1, group1, none, none, group2, group2, none, group3, none]
  let tabs = createManyTabs(10);
  [tabs[0], tabs[1], tabs[2]].forEach(t => {
    gBrowser.addToMultiSelectedTabs(t);
    ok(t.multiselected, "added tab to mutliselection");
  });
  gBrowser.addTabGroup([tabs[0], tabs[1], tabs[2]], { insertBefore: tabs[0] });
  [tabs[0], tabs[1], tabs[2]].forEach(t => {
    ok(!t.multiselected, "tab no longer multiselected after adding to group");
  });
  gBrowser.addTabGroup([tabs[5], tabs[6]], { insertBefore: tabs[5] });
  gBrowser.addTabGroup([tabs[8]], { insertBefore: tabs[8] });

  // Click the first tab in our test group to make sure the default tab at the
  // start of the tab strip is deselected
  EventUtils.synthesizeMouseAtCenter(tabs[1], {});

  // select a few tabs, both in and out of groups
  [tabs[3], tabs[6], tabs[8]].forEach(t => {
    gBrowser.addToMultiSelectedTabs(t);
  });

  let tabToClick = tabs[3];

  await withTabMenu(tabToClick, async (_m1, _m2, ungroupTabItem) => {
    Assert.ok(!ungroupTabItem.hidden, "ungroupTabItem is visible");

    ungroupTabItem.click();
  });

  Assert.ok(!tabs[1].group, "group1 tab is no longer in group");
  Assert.ok(!tabs[6].group, "group2 tab is no longer in group");
  Assert.ok(!tabs[8].group, "group3 tab is no longer in group");

  Assert.equal(
    tabs[1],
    gBrowser.tabs[3],
    "ungrouped tab from group1 is adjacent to group1"
  );
  Assert.equal(
    tabs[6],
    gBrowser.tabs[7],
    "ungrouped tab from group2 has not changed position"
  );
  Assert.equal(
    tabs[8],
    gBrowser.tabs[9],
    "ungrouped tab from group3 has not changed position"
  );

  tabs.forEach(t => {
    BrowserTestUtils.removeTab(t);
  });
});

// Context menu tests: "new tab to right" option
// ---

/**
 * Tests that the "new tab to right" context menu option will create the new
 * tab inside of the same tab group as the context menu tab when the insertion
 * point is between two tabs within the same tab group
 */
add_task(async function test_newTabToRightInsideGroup() {
  let [tab1, tab2, tab3] = createManyTabs(3);
  let group = gBrowser.addTabGroup([tab1, tab2, tab3]);

  await withNewTabFromTabMenu(tab2, newTab => {
    Assert.equal(newTab.group, group, "new tab should be in the tab group");
  });

  await removeTabGroup(group);
});

/**
 * Tests that the "new tab to right" context menu option will create the new
 * tab inside of the same tab group as the context menu tab when the context
 * menu tab is the last tab in the tab group
 */
add_task(async function test_newTabToRightAtEndOfGroup() {
  let [tab1, tab2, tab3] = createManyTabs(3);
  let group = gBrowser.addTabGroup([tab1, tab2, tab3]);

  await withNewTabFromTabMenu(tab3, newTab => {
    Assert.equal(newTab.group, group, "new tab should be in the tab group");
  });

  await removeTabGroup(group);
});

/**
 * Tests that the "new tab to right" context menu option will create the new
 * tab outside of any tab group when then context menu tab is to the left of
 * a tab that is inside of a tab group
 */
add_task(async function test_newTabToRightBeforeGroup() {
  let [tab1, tab2, tab3] = createManyTabs(3);
  let group = gBrowser.addTabGroup([tab2, tab3], { insertBefore: tab2 });

  await withNewTabFromTabMenu(tab1, async newTab => {
    Assert.ok(!newTab.group, "new tab should not be in a tab group");
  });

  await removeTabGroup(group);
  await BrowserTestUtils.removeTab(tab1);
});

// Opening new tabs from links around/within tab groups
// ---

const PAGE_WITH_LINK_URI =
  "https://example.com/browser/browser/components/tabbrowser/test/browser/tabs/file_new_tab_page.html";
const LINK_ID_SELECTOR = "#link_to_example_com";

/**
 * @returns {Promise<MozTabbrowserTab>}
 */
async function getNewTabFromLink() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.opentabfor.middleclick", true],
      ["browser.tabs.insertRelatedAfterCurrent", true],
    ],
  });

  let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, null, true);
  await BrowserTestUtils.synthesizeMouseAtCenter(
    LINK_ID_SELECTOR,
    { button: 1 },
    gBrowser.selectedBrowser
  );
  let newTab = await newTabPromise;

  return newTab;
}

/**
 * Tests that for a tab inside of a tab group, opening a link on the
 * page in a new tab will open the new tab inside the tab group
 */
add_task(async function test_openLinkInNewTabInsideGroup() {
  let tab1 = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    skipAnimation: true,
  });
  let tabWithLink = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    PAGE_WITH_LINK_URI
  );
  let tab2 = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    skipAnimation: true,
  });
  let group = gBrowser.addTabGroup([tab1, tabWithLink, tab2]);

  const newTab = await getNewTabFromLink();
  Assert.equal(
    newTab.group,
    group,
    "new tab should be in the same tab group as the opening page"
  );

  await removeTabGroup(group);
});

/**
 * Tests that for the last tab inside of a tab group, opening a link on the
 * page in a new tab will open the new tab inside the tab group
 */
add_task(async function test_openLinkInNewTabAtEndOfGroup() {
  let [tab1, tab2] = createManyTabs(2);
  let tabWithLink = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    PAGE_WITH_LINK_URI
  );
  let group = gBrowser.addTabGroup([tab1, tab2, tabWithLink]);

  const newTab = await getNewTabFromLink();
  Assert.equal(
    newTab.group,
    group,
    "new tab should be in the same tab group as the opening page"
  );

  await removeTabGroup(group);
});

/**
 * Tests that for a standalone tab to the left of a tab group, opening a link
 * on the page in a new tab will NOT open the new tab inside the tab group
 */
add_task(async function test_openLinkInNewTabBeforeGroup() {
  let tabWithLink = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    PAGE_WITH_LINK_URI
  );
  let [tab1, tab2] = createManyTabs(2);
  let group = gBrowser.addTabGroup([tab1, tab2]);
  gBrowser.selectedTab = tabWithLink;

  const newTab = await getNewTabFromLink();
  Assert.ok(
    !newTab.group,
    "new tab should not be in a group because the opening tab was not in a group"
  );

  await removeTabGroup(group);
  BrowserTestUtils.removeTab(tabWithLink);
  BrowserTestUtils.removeTab(newTab);
});

/*
 * Tests that gBrowser.tabs does not contain tab groups after tabs have been
 * moved between tab groups. Resolves bug1920731.
 */
add_task(async function test_tabsContainNoTabGroups() {
  let tab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    skipAnimation: true,
  });

  let group1 = gBrowser.addTabGroup([tab]);
  gBrowser.addTabGroup([tab]);

  Assert.equal(
    gBrowser.tabs.length,
    2,
    "tab strip contains default tab and our tab"
  );
  gBrowser.tabs.forEach((t, idx) => {
    Assert.equal(
      t.constructor.name,
      "MozTabbrowserTab",
      `gBrowser.tabs[${idx}] is of type MozTabbrowserTab`
    );
  });

  BrowserTestUtils.removeTab(tab);
  await removeTabGroup(group1);
});

/**
 * Tests behavior of the group management panel.
 */
add_task(async function test_tabGroupCreatePanel() {
  const triggerHandler = sinon.stub();
  const tabGroupCreatedTrigger =
    ASRouterTriggerListeners.get("tabGroupCreated");
  tabGroupCreatedTrigger.uninit();
  tabGroupCreatedTrigger.init(triggerHandler);

  let tabgroupEditor = document.getElementById("tab-group-editor");
  let tabgroupPanel = tabgroupEditor.panel;
  let nameField = tabgroupPanel.querySelector("#tab-group-name");
  let tab = BrowserTestUtils.addTab(gBrowser, "about:blank");

  let openCreatePanel = async () => {
    let panelShown = BrowserTestUtils.waitForPopupEvent(tabgroupPanel, "shown");
    let group = gBrowser.addTabGroup([tab], {
      color: "cyan",
      label: "Food",
      isUserTriggered: true,
    });
    await panelShown;
    return group;
  };

  let group = await openCreatePanel();
  Assert.equal(tabgroupPanel.state, "open", "Create panel is visible");
  Assert.ok(tabgroupEditor.createMode, "Group editor is in create mode");
  // Edit panel should be populated with correct group details
  Assert.equal(
    document.activeElement,
    nameField,
    "Create panel's input is focused initially"
  );
  Assert.equal(
    nameField.value,
    group.label,
    "Create panel's input populated with correct name"
  );
  Assert.equal(
    tabgroupPanel.querySelector("input[name='tab-group-color']:checked").value,
    group.color,
    "Create panel's colorpicker has correct color pre-selected"
  );

  info("New group should be removed after hitting Cancel");
  let panelHidden = BrowserTestUtils.waitForPopupEvent(tabgroupPanel, "hidden");
  let cancelButton = tabgroupPanel.querySelector(
    "#tab-group-editor-button-cancel"
  );
  if (AppConstants.platform == "macosx") {
    cancelButton.click();
  } else {
    cancelButton.focus();
    EventUtils.synthesizeKey("VK_RETURN");
  }
  await panelHidden;
  Assert.ok(!tab.group, "Tab is ungrouped after hitting Cancel");

  info("New group should be removed after hitting Esc");
  group = await openCreatePanel();
  panelHidden = BrowserTestUtils.waitForPopupEvent(tabgroupPanel, "hidden");
  EventUtils.synthesizeKey("KEY_Escape");
  await panelHidden;
  Assert.ok(!tab.group, "Tab is ungrouped after hitting Esc");

  info("New group should remain when dismissing panel");
  group = await openCreatePanel();
  panelHidden = BrowserTestUtils.waitForPopupEvent(tabgroupPanel, "hidden");
  tabgroupPanel.hidePopup();
  await panelHidden;
  Assert.equal(tabgroupPanel.state, "closed", "Tabgroup edit panel is closed");
  Assert.equal(group.label, "Food");
  Assert.equal(group.color, "cyan");
  group.ungroupTabs();

  info("Panel inputs should work correctly");
  group = await openCreatePanel();
  nameField.focus();
  nameField.value = "";
  EventUtils.sendString("Shopping");
  Assert.equal(
    group.label,
    "Shopping",
    "Group label changed when input value changed"
  );
  tabgroupPanel.querySelector("#tab-group-editor-swatch-red").click();
  Assert.equal(
    group.color,
    "red",
    "Group color changed to red after clicking red swatch"
  );
  Assert.equal(
    tabgroupPanel.querySelector("input[name='tab-group-color']:checked").value,
    "red",
    "Red swatch radio selected after clicking red swatch"
  );

  info(
    "Panel should be dismissed after clicking Create and new group should remain"
  );
  panelHidden = BrowserTestUtils.waitForPopupEvent(tabgroupPanel, "hidden");
  let done = BrowserTestUtils.waitForEvent(
    tabgroupEditor,
    "TabGroupCreateDone"
  );
  tabgroupPanel.querySelector("#tab-group-editor-button-create").click();
  await panelHidden;
  await done;
  Assert.ok(triggerHandler.called, "Called after tab group created");
  Assert.equal(tabgroupPanel.state, "closed", "Tabgroup edit panel is closed");
  Assert.equal(group.label, "Shopping");
  Assert.equal(group.color, "red");

  let rightClickGroupLabel = async () => {
    info("right-clicking on the group label should reopen panel in edit mode");
    let panelShown = BrowserTestUtils.waitForPopupEvent(tabgroupPanel, "shown");
    EventUtils.synthesizeMouseAtCenter(
      group.querySelector(".tab-group-label"),
      { type: "contextmenu", button: 2 },
      window
    );
    await panelShown;
    Assert.equal(tabgroupPanel.state, "open", "Tabgroup edit panel is open");
    Assert.ok(!tabgroupEditor.createMode, "Group editor is not in create mode");
  };

  info("Panel should be dismissed after hitting Enter and group should remain");
  await rightClickGroupLabel();
  panelHidden = BrowserTestUtils.waitForPopupEvent(tabgroupPanel, "hidden");
  EventUtils.synthesizeKey("VK_RETURN");
  await panelHidden;
  Assert.equal(tabgroupPanel.state, "closed", "Tabgroup edit panel is closed");
  Assert.equal(group.label, "Shopping");
  Assert.equal(group.color, "red");

  await rightClickGroupLabel();
  info("Esc key should should close the edit panel");
  panelHidden = BrowserTestUtils.waitForPopupEvent(tabgroupPanel, "hidden");
  EventUtils.synthesizeKey("KEY_Escape");
  await panelHidden;
  Assert.equal(tabgroupPanel.state, "closed", "Tabgroup edit panel is closed");
  Assert.equal(group.label, "Shopping");
  Assert.equal(group.color, "red");

  await rightClickGroupLabel();
  info("Removing group via delete button");
  panelHidden = BrowserTestUtils.waitForPopupEvent(tabgroupPanel, "hidden");
  let removePromise = BrowserTestUtils.waitForEvent(group, "TabGroupRemoved");
  let deleteButton = tabgroupPanel.querySelector("#tabGroupEditor_deleteGroup");
  if (AppConstants.platform == "macosx") {
    deleteButton.click();
  } else {
    deleteButton.focus();
    EventUtils.synthesizeKey("VK_RETURN");
  }
  await Promise.all([panelHidden, removePromise]);
  tabGroupCreatedTrigger.uninit();
});

async function createTabGroupAndOpenEditPanel(tabs = [], label = "") {
  let tabgroupEditor = document.getElementById("tab-group-editor");
  let tabgroupPanel = tabgroupEditor.panel;
  if (!tabs.length) {
    let tab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
      animate: false,
    });
    tabs = [tab];
  }
  let group = gBrowser.addTabGroup(tabs, { color: "cyan", label });

  let panelShown = BrowserTestUtils.waitForPopupEvent(tabgroupPanel, "shown");
  EventUtils.synthesizeMouseAtCenter(
    group.querySelector(".tab-group-label"),
    { type: "contextmenu", button: 2 },
    window
  );
  return new Promise(resolve => {
    panelShown.then(() => {
      resolve({ tabgroupEditor, group });
    });
  });
}

add_task(async function test_tabGroupPanelAddTab() {
  let { tabgroupEditor, group } = await createTabGroupAndOpenEditPanel(
    [],
    "test_tabGroupPanelAddTab"
  );
  let tabgroupPanel = tabgroupEditor.panel;

  let addNewTabButton = tabgroupPanel.querySelector(
    "#tabGroupEditor_addNewTabInGroup"
  );

  Assert.equal(group.tabs.length, 1, "Group has 1 tab");
  let panelHidden = BrowserTestUtils.waitForPopupEvent(tabgroupPanel, "hidden");
  addNewTabButton.click();
  await panelHidden;
  Assert.ok(tabgroupPanel.state === "closed", "Group editor is closed");
  Assert.equal(group.tabs.length, 2, "Group has 2 tabs");

  await removeTabGroup(group);
});

add_task(async function test_tabGroupPanelUngroupTabs() {
  let { tabgroupEditor, group } = await createTabGroupAndOpenEditPanel(
    [],
    "test_tabGroupPanelAddTab"
  );
  let tabgroupPanel = tabgroupEditor.panel;
  let tab = group.tabs[0];
  let ungroupTabsButton = tabgroupPanel.querySelector(
    "#tabGroupEditor_ungroupTabs"
  );

  Assert.ok(tab.group.id == group.id, "Tab is in group");
  let panelHidden = BrowserTestUtils.waitForPopupEvent(tabgroupPanel, "hidden");
  ungroupTabsButton.click();
  await panelHidden;
  Assert.ok(!tab.group, "Tab is no longer grouped");

  BrowserTestUtils.removeTab(tab);
});

/**
 * Tests that the "move group to new window" correctly moves a group
 * to a new window, preserving tab selection and order.
 */
add_task(async function test_moveGroupToNewWindow() {
  let tabs = [
    BrowserTestUtils.addTab(gBrowser, "about:cache", {
      skipAnimation: true,
    }),
    BrowserTestUtils.addTab(gBrowser, "about:robots", {
      skipAnimation: true,
    }),
    BrowserTestUtils.addTab(gBrowser, "about:mozilla", {
      skipAnimation: true,
    }),
  ];
  gBrowser.selectedTab = tabs[1];
  let assertTabsInCorrectOrder = tabsToCheck => {
    Assert.equal(
      tabsToCheck[0].linkedBrowser.currentURI.spec,
      "about:cache",
      "about:cache is first"
    );
    Assert.equal(
      tabsToCheck[1].linkedBrowser.currentURI.spec,
      "about:robots",
      "about:robots is second"
    );
    Assert.equal(
      tabsToCheck[2].linkedBrowser.currentURI.spec,
      "about:mozilla",
      "about:mozilla is third"
    );
  };
  let { group } = await createTabGroupAndOpenEditPanel(
    tabs,
    "test_moveGroupToNewWindow"
  );

  let newWindowOpened = BrowserTestUtils.waitForNewWindow();
  document.getElementById("tabGroupEditor_moveGroupToNewWindow").click();
  let newWin = await newWindowOpened;
  Assert.ok(newWin != window, "Group is moved to new window");

  let movedTabs = newWin.gBrowser.tabs;
  let movedGroup = movedTabs[0].group;
  Assert.equal(movedGroup.id, group.id, "Tab is in original group");

  Assert.equal(
    newWin.gBrowser.selectedTab,
    newWin.gBrowser.tabs[1],
    "Second tab remains selected"
  );
  assertTabsInCorrectOrder(newWin.gBrowser.tabs);
  let tabgroupEditor = newWin.document.getElementById("tab-group-editor");
  let panelOpen = BrowserTestUtils.waitForPopupEvent(
    tabgroupEditor.panel,
    "shown"
  );
  tabgroupEditor.openEditModal(movedGroup);
  await panelOpen;

  let moveGroupButton = newWin.document.getElementById(
    "tabGroupEditor_moveGroupToNewWindow"
  );
  Assert.ok(
    moveGroupButton.disabled,
    "Button is disabled when group is only thing in window"
  );

  let panelHidden = BrowserTestUtils.waitForPopupEvent(
    tabgroupEditor.panel,
    "hidden"
  );
  tabgroupEditor.panel.hidePopup();
  await panelHidden;

  BrowserTestUtils.addTab(newWin.gBrowser, "about:blank", {
    skipAnimation: true,
  });
  panelOpen = BrowserTestUtils.waitForPopupEvent(tabgroupEditor.panel, "shown");
  tabgroupEditor.openEditModal(movedGroup);
  await panelOpen;
  Assert.ok(
    !moveGroupButton.disabled,
    "Button is enabled again when additional tab present"
  );
  await removeTabGroup(movedGroup);
  await BrowserTestUtils.closeWindow(newWin, { animate: false });
});

/**
 * The "save & close" button in the tabgroup menu should be disabled if the
 * group is not saveable.
 */
add_task(async function test_saveDisabledForUnimportantGroup() {
  let { tabgroupEditor, group } = await createTabGroupAndOpenEditPanel(
    [],
    "test_saveDisabledForUnimportantGroups"
  );
  let saveAndCloseGroupButton = tabgroupEditor.panel.querySelector(
    "#tabGroupEditor_saveAndCloseGroup"
  );
  Assert.ok(
    saveAndCloseGroupButton.disabled,
    "Save button is disabled for newtab-only group"
  );
  let panelHidden = BrowserTestUtils.waitForPopupEvent(
    tabgroupEditor.panel,
    "hidden"
  );
  tabgroupEditor.panel.hidePopup();
  await panelHidden;
  await removeTabGroup(group);
});

add_task(async function test_saveAndCloseGroup() {
  const triggerHandler = sinon.stub();
  const tabGroupSavedTrigger = ASRouterTriggerListeners.get("tabGroupSaved");
  tabGroupSavedTrigger.uninit();
  tabGroupSavedTrigger.init(triggerHandler);

  let tab = await addTab("about:mozilla");
  let { tabgroupEditor, group } = await createTabGroupAndOpenEditPanel(
    [tab],
    "test_saveAndCloseGroup"
  );
  let tabgroupPanel = tabgroupEditor.panel;
  await TabStateFlusher.flush(tab.linkedBrowser);
  let saveAndCloseGroupButton = tabgroupPanel.querySelector(
    "#tabGroupEditor_saveAndCloseGroup"
  );

  Assert.ok(gBrowser.getTabGroupById(group.id), "Group exists in browser");

  let events = [
    BrowserTestUtils.waitForPopupEvent(tabgroupPanel, "hidden"),
    BrowserTestUtils.waitForEvent(group, "TabGroupSaved"),
    BrowserTestUtils.waitForEvent(group, "TabGroupRemoved"),
  ];
  saveAndCloseGroupButton.click();
  await Promise.all(events);

  Assert.ok(triggerHandler.calledOnce, "Called once after tab group saved");
  Assert.ok(
    !gBrowser.getTabGroupById(group.id),
    "Group was removed from browser"
  );
  Assert.ok(SessionStore.getSavedTabGroup(group.id), "Group is in savedGroups");

  SessionStore.forgetSavedTabGroup(group.id);

  BrowserTestUtils.removeTab(tab);
  tabGroupSavedTrigger.uninit();
});

add_task(async function test_saveAndCloseGroupViaMiddleClick() {
  let tab = await addTab("about:mozilla");
  let group = gBrowser.addTabGroup([tab]);
  await TabStateFlusher.flush(tab.linkedBrowser);

  Assert.ok(gBrowser.getTabGroupById(group.id), "Group exists in browser");

  let events = [
    BrowserTestUtils.waitForEvent(group, "TabGroupSaved"),
    BrowserTestUtils.waitForEvent(group, "TabGroupRemoved"),
  ];
  triggerMiddleClickOn(group.labelElement);
  await Promise.all(events);

  Assert.ok(
    !gBrowser.getTabGroupById(group.id),
    "Group was removed from browser"
  );
  Assert.ok(SessionStore.getSavedTabGroup(group.id), "Group is in savedGroups");

  SessionStore.forgetSavedTabGroup(group.id);
});

add_task(async function test_pinningInteractionsWithTabGroups() {
  const tabs = createManyTabs(3);
  let group = gBrowser.addTabGroup(tabs, { insertBefore: gBrowser.tabs[0] });
  const workingTab = tabs[1];

  Assert.equal(workingTab.group, group, "tab is in group");
  gBrowser.pinTab(workingTab);
  Assert.ok(!workingTab.group, "pinned tab is no longer in the tab group");
  Assert.notEqual(
    group.previousElementSibling,
    workingTab,
    "pinned tab should not be before the tab group"
  );

  gBrowser.unpinTab(workingTab);
  Assert.ok(!workingTab.group, "unpinned tab is still not in the tab group");
  Assert.equal(
    group.previousElementSibling,
    workingTab,
    "unpinned tab is still before before the tab group"
  );

  const moreTabs = createManyTabs(5);
  moreTabs.forEach(tab => gBrowser.pinTab(tab));
  Assert.ok(
    !moreTabs.some(tab => !!tab.group),
    "none of the new pinned tabs are in the tab group"
  );

  const firstPinnedTabToUnpin = gBrowser.tabs[0];
  const tabContainer = document.getElementById("tabbrowser-arrowscrollbox");
  gBrowser.unpinTab(firstPinnedTabToUnpin);
  Assert.ok(
    !firstPinnedTabToUnpin.group,
    "unpinned tab is not in the tab group"
  );
  Assert.equal(
    tabContainer.firstChild,
    firstPinnedTabToUnpin,
    "unpinned tab is the first tab after all of the pinned tabs"
  );

  moreTabs.concat(tabs).forEach(tab => BrowserTestUtils.removeTab(tab));
  await removeTabGroup(group);
});

add_task(async function test_pinFirstGroupedTab() {
  let tabs = [gBrowser.selectedTab, ...createManyTabs(1)];
  let group = gBrowser.addTabGroup(tabs);

  gBrowser.pinTab(tabs[0]);
  Assert.ok(tabs[0].pinned, "pinned first tab of group");
  Assert.ok(!tabs[0].group, "pinning first tab ungroups it");
  gBrowser.unpinTab(tabs[0]);

  await removeTabGroup(group);
});

add_task(async function test_adoptTab() {
  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  let tabs = createManyTabs(3, newWin);
  let group = newWin.gBrowser.addTabGroup(tabs);

  let otherWinTab = BrowserTestUtils.addTab(gBrowser, "about:robots", {
    skipAnimation: true,
  });
  let adoptedTab = newWin.gBrowser.adoptTab(otherWinTab, { tabIndex: 1 });

  Assert.equal(adoptedTab._tPos, 1, "tab adopted into expected position");
  Assert.equal(adoptedTab.group, group, "tab adopted into tab group");

  await removeTabGroup(group);
  await BrowserTestUtils.closeWindow(newWin, { animate: false });
});

add_task(async function test_insertAfterCurrent() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.insertAfterCurrent", true]],
  });

  let group = gBrowser.addTabGroup([gBrowser.selectedTab]);
  let extraTab = BrowserTestUtils.addTab(gBrowser, "about:robots", {
    skipAnimation: true,
  });
  let newTabPromise = BrowserTestUtils.waitForEvent(window, "TabOpen");
  BrowserCommands.openTab();
  let { target: newTab } = await newTabPromise;

  Assert.equal(newTab.group, group, "new tab added to current group");
  Assert.equal(
    group.tabs.indexOf(newTab),
    1,
    "new tab added as second tab to group"
  );

  group.ungroupTabs();
  BrowserTestUtils.removeTab(newTab);
  BrowserTestUtils.removeTab(extraTab);

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_bug1936015() {
  // Checks that groups are properly deleted if the group was created before
  // the user switches between vertical and horizontal tabs.
  const tabs = createManyTabs(3);
  let group = gBrowser.addTabGroup(tabs);

  SpecialPowers.pushPrefEnv({
    set: [
      ["sidebar.revamp", true],
      ["sidebar.verticalTabs", true],
    ],
  });

  await removeTabGroup(group);

  Assert.ok(!group.parentNode, "Group has been fully removed from DOM");

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_bug1957723_addTabsByIndex() {
  let initialTab = gBrowser.tabs[0];
  let triggeringPrincipal = Services.scriptSecurityManager.getSystemPrincipal();

  const tabs = createManyTabs(5);
  const tabGroup = gBrowser.addTabGroup([tabs[1], tabs[2], tabs[3]], {
    insertBefore: tabs[1],
  });

  let tab1 = gBrowser.addTab("https://example.com", {
    tabIndex: 2,
    triggeringPrincipal,
  });
  Assert.equal(
    tab1._tPos,
    2,
    "Tab added at starting index of tab group is in correct position"
  );
  Assert.equal(
    tab1.group,
    null,
    "Tab added at starting index of tab group is not in group"
  );
  gBrowser.removeTab(tab1);

  let tab2 = gBrowser.addTab("https://example.com", {
    tabIndex: 4,
    triggeringPrincipal,
  });
  Assert.equal(
    tab2._tPos,
    4,
    "Tab added by index just before end of tab group is in correct position"
  );
  Assert.equal(
    tab2.group.id,
    tabGroup.id,
    "Tab added by index just before end of tab group is in group"
  );
  gBrowser.removeTab(tab2);

  let tab3 = gBrowser.addTab("https://example.com", {
    tabIndex: 5,
    triggeringPrincipal,
  });
  Assert.equal(
    tab3._tPos,
    5,
    "Tab added at index just after end of tab group is in correct position"
  );
  Assert.equal(
    tab3.group,
    null,
    "Tab added at index just after end of tab group is not in group"
  );
  gBrowser.removeTab(tab3);

  gBrowser.removeAllTabsBut(initialTab);
});

add_task(async function test_bug1959438_duplicateTabJustBeforeGroup() {
  let initialTab = gBrowser.tabs[0];
  let triggeringPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
  const tabs = createManyTabs(3);

  gBrowser.addTabGroup(tabs);

  Assert.equal(gBrowser.tabs.length, 4, "Tab strip starts with four tabs");

  gBrowser.selectTabAtIndex(0);

  // Simulate an addTab call similar to what would be called when a tab is
  // duplicated. This produces a situation where addTab has no index, but knows
  // it needs to create one next to the currently selected tab, and guesses for
  // itself.
  // If this happens next to a tab group, the resulting element index will
  // point to the tab group label.
  gBrowser.addTab("https://example.com", {
    tabIndex: undefined,
    relatedToCurrent: true,
    ownerTab: gBrowser.selectedTab,
    triggeringPrincipal,
  });

  // This will fail if the tab ends up merged with the tab label.
  Assert.equal(gBrowser.tabs.length, 5, "A new tab was added to the tab strip");

  gBrowser.removeAllTabsBut(initialTab);
});
