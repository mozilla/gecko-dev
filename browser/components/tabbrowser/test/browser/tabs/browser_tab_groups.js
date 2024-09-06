/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_tabGroupCreateAndAddTab() {
  let tab1 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let group = gBrowser.addTabGroup("blue", "test", [tab1]);

  Assert.ok(group.id, "group has id");
  Assert.ok(group.tabs.includes(tab1), "tab1 is in group");

  let tab2 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  group.addTabs([tab2]);

  Assert.equal(group.tabs.length, 2, "group has 2 tabs");
  Assert.ok(group.tabs.includes(tab2), "tab1 is in group");

  gBrowser.removeTabGroup(group);
});

add_task(async function test_getTabGroups() {
  let tab1 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let group1 = gBrowser.addTabGroup("blue", "test1", [tab1]);
  Assert.equal(
    gBrowser.tabGroups.length,
    1,
    "there is one group in the tabstrip"
  );

  let tab2 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let group2 = gBrowser.addTabGroup("red", "test2", [tab2]);
  Assert.equal(
    gBrowser.tabGroups.length,
    2,
    "there are two groups in the tabstrip"
  );

  gBrowser.removeTabGroup(group1);
  gBrowser.removeTabGroup(group2);
  Assert.equal(
    gBrowser.tabGroups.length,
    0,
    "there are no groups in the tabstrip"
  );
});

add_task(async function test_tabGroupCollapseAndExpand() {
  let tab1 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let group = gBrowser.addTabGroup("blue", "test", [tab1]);

  Assert.ok(!group.collapsed, "group is expanded by default");

  group.querySelector(".tab-group-label").click();
  Assert.ok(group.collapsed, "group is collapsed on click");

  group.querySelector(".tab-group-label").click();
  Assert.ok(!group.collapsed, "collapsed group is expanded on click");

  gBrowser.removeTabGroup(group);
});

add_task(async function test_tabGroupCollapsedTabsNotVisible() {
  let tab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let group = gBrowser.addTabGroup("blue", "test", [tab]);

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
  gBrowser.removeTabGroup(group);
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
  let group = gBrowser.addTabGroup("blue", "test", [tabInGroup]);
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
  gBrowser.removeTabGroup(group);
});

/*
 * Tests that if a tab group is collapsed while the selected tab is in the group,
 * the selected tab will change to be the adjacent tab just before the group,
 * if no tabs exist after the group
 */
add_task(async function test_tabGroupCollapseSelectsAdjacentTabBefore() {
  let adjacentTabBefore = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tabInGroup = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let group = gBrowser.addTabGroup("blue", "test", [tabInGroup]);

  gBrowser.selectedTab = tabInGroup;

  group.collapsed = true;
  Assert.equal(
    gBrowser.selectedTab,
    adjacentTabBefore,
    "selected tab becomes adjacent tab after group on collapse"
  );

  BrowserTestUtils.removeTab(adjacentTabBefore);
  group.collapsed = false;
  gBrowser.removeTabGroup(group);
});

add_task(async function test_tabGroupCollapseCreatesNewTabIfAllTabsInGroup() {
  // This test has to be run in a new window because there is currently no
  // API to remove a tab from a group, which breaks tests following this one
  // This can be removed once the group remove API is implemented
  let fgWindow = await BrowserTestUtils.openNewBrowserWindow();

  let group = fgWindow.gBrowser.addTabGroup(
    "blue",
    "test",
    fgWindow.gBrowser.tabs
  );

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
  fgWindow.gBrowser.removeTabGroup(group);
  await BrowserTestUtils.closeWindow(fgWindow);
});

add_task(async function test_tabUngroup() {
  let extraTab1 = BrowserTestUtils.addTab(gBrowser, "about:blank");

  let groupedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let group = gBrowser.addTabGroup("blue", "test", [groupedTab]);

  let extraTab2 = BrowserTestUtils.addTab(gBrowser, "about:blank");

  Assert.equal(groupedTab._tPos, 2, "grouped tab starts in correct position");
  Assert.equal(groupedTab.group, group, "tab belongs to group");

  group.ungroupTabs();

  Assert.equal(
    groupedTab._tPos,
    2,
    "tab is in the same position as before ungroup"
  );
  Assert.equal(groupedTab.group, null, "tab no longer belongs to group");

  // TODO add a DOM event that fires when tab group is removed and listen for that here
  await BrowserTestUtils.waitForCondition(() => {
    return group.parentElement === null;
  });
  Assert.equal(group.parentElement, null, "group is unloaded");

  BrowserTestUtils.removeTab(groupedTab);
  BrowserTestUtils.removeTab(extraTab1);
  BrowserTestUtils.removeTab(extraTab2);
});

add_task(async function test_tabGroupRemove() {
  let groupedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let group = gBrowser.addTabGroup("blue", "test", [groupedTab]);

  gBrowser.removeTabGroup(group);

  Assert.equal(groupedTab.parentElement, null, "grouped tab is unloaded");
  Assert.equal(group.parentElement, null, "group is unloaded");
});

add_task(async function test_tabGroupDeletesWhenLastTabClosed() {
  let tab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let group = gBrowser.addTabGroup("blue", "test", [tab]);

  gBrowser.removeTab(tab);

  Assert.equal(group.parent, null, "group is removed from tabbrowser");
});

add_task(async function test_tabGroupMoveToNewWindow() {
  let tabUri = "https://example.com/tab-group-test";
  let groupedTab = BrowserTestUtils.addTab(gBrowser, tabUri);
  let group = gBrowser.addTabGroup("blue", "test", [groupedTab]);

  let fgWindow = await BrowserTestUtils.openNewBrowserWindow();
  fgWindow.gBrowser.adoptTabGroup(group, 0);

  // TODO add a DOM event that fires when tab group is removed and listen for that here
  await BrowserTestUtils.waitForCondition(() => {
    return group.parentElement === null;
  });

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

  fgWindow.gBrowser.removeTabGroup(group);
  await BrowserTestUtils.closeWindow(fgWindow);
});

add_task(async function test_TabGroupEvents() {
  let tab1 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab2 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let group;

  let createdGroupId = null;
  let tabGroupCreated = BrowserTestUtils.waitForEvent(
    window,
    "TabGroupCreate"
  ).then(event => {
    createdGroupId = event.target.id;
  });
  group = gBrowser.addTabGroup("blue", "test", [tab1]);
  await tabGroupCreated;
  Assert.equal(
    createdGroupId,
    group.id,
    "TabGroupCreate fired with correct group as target"
  );

  let groupedGroupId = null;
  let tabGrouped = BrowserTestUtils.waitForEvent(tab2, "TabGrouped").then(
    event => {
      groupedGroupId = event.detail.id;
    }
  );
  group.addTabs([tab2]);
  await tabGrouped;
  Assert.equal(groupedGroupId, group.id, "TabGrouped fired with correct group");

  let groupCollapsed = BrowserTestUtils.waitForEvent(group, "TabGroupCollapse");
  group.collapsed = true;
  await groupCollapsed;

  let groupExpanded = BrowserTestUtils.waitForEvent(group, "TabGroupExpand");
  group.collapsed = false;
  await groupExpanded;

  let ungroupedGroupId = null;
  let tabUngrouped = BrowserTestUtils.waitForEvent(tab2, "TabUngrouped").then(
    event => {
      ungroupedGroupId = event.detail.id;
    }
  );
  gBrowser.moveTabTo(tab2, 0);
  await tabUngrouped;
  Assert.equal(
    ungroupedGroupId,
    group.id,
    "TabUngrouped fired with correct group"
  );

  let tabGroupRemoved = BrowserTestUtils.waitForEvent(group, "TabGroupRemove");
  gBrowser.removeTabGroup(group);
  await tabGroupRemoved;

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});

add_task(async function test_moveTabBetweenGroups() {
  let tab1 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab2 = BrowserTestUtils.addTab(gBrowser, "about:blank");

  let tab1Added = BrowserTestUtils.waitForEvent(tab1, "TabGrouped");
  let tab2Added = BrowserTestUtils.waitForEvent(tab2, "TabGrouped");
  let group1 = gBrowser.addTabGroup("blue", "test1", [tab1]);
  let group2 = gBrowser.addTabGroup("red", "test2", [tab2]);
  await Promise.allSettled([tab1Added, tab2Added]);

  let ungroupedGroupId = null;
  let tabUngrouped = BrowserTestUtils.waitForEvent(tab1, "TabUngrouped").then(
    event => {
      ungroupedGroupId = event.detail.id;
    }
  );
  let groupedGroupId = null;
  let tabGrouped = BrowserTestUtils.waitForEvent(tab1, "TabGrouped").then(
    event => {
      groupedGroupId = event.detail.id;
      Assert.ok(ungroupedGroupId, "TabUngrouped fires before TabGrouped");
    }
  );

  group2.addTabs([tab1]);
  await Promise.allSettled([tabUngrouped, tabGrouped]);
  Assert.equal(ungroupedGroupId, group1.id, "TabUngrouped fired with group1");
  Assert.equal(groupedGroupId, group2.id, "TabGrouped fired with group2");

  Assert.ok(
    !group1.parent,
    "group1 has been removed after losing its last tab"
  );
  Assert.equal(group2.tabs.length, 2, "group2 has 2 tabs");

  gBrowser.removeTabGroup(group2);
});
