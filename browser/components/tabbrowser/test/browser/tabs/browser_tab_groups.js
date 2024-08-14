/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_tabGroups() {
  let group = gBrowser.addTabGroup("blue", "test");

  Assert.ok(group.id, "group has id");

  let tab1 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  group.addTabs([tab1]);

  Assert.ok(group.tabs.includes(tab1), "tab1 is in group");

  // TODO add API to remove group
  BrowserTestUtils.removeTab(tab1);
  group.remove();
});

add_task(async function test_tabGroupCollapseAndExpand() {
  let group = gBrowser.addTabGroup("blue", "test");
  let tab1 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  group.addTabs([tab1]);

  Assert.ok(!group.collapsed, "group is expanded by default");

  group.querySelector(".tab-group-label").click();
  Assert.ok(group.collapsed, "group is collapsed on click");

  group.querySelector(".tab-group-label").click();
  Assert.ok(!group.collapsed, "collapsed group is expanded on click");

  // TODO add API to remove group
  BrowserTestUtils.removeTab(tab1);
  group.remove();
});

add_task(async function test_tabGroupCollapsedTabsNotVisible() {
  let group = gBrowser.addTabGroup("blue", "test");
  let tab1 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  group.addTabs([tab1]);

  Assert.ok(!group.collapsed, "group is expanded by default");

  Assert.ok(
    gBrowser.visibleTabs.includes(tab1),
    "tab in expanded tab group is visible"
  );

  group.collapsed = true;
  Assert.ok(
    !gBrowser.visibleTabs.includes(tab1),
    "tab in collapsed tab group is not visible"
  );

  // TODO add API to remove group
  // TODO BrowserTestUtils.removeTab breaks if the tab is not in a visible state
  group.collapsed = false;
  BrowserTestUtils.removeTab(tab1);
  group.remove();
});

/*
 * Tests that if a tab group is collapsed while the selected tab is in the group,
 * the selected tab will change to be the adjacent tab just after the group.
 *
 * This tests that the tab after the group will be prioritized over the tab
 * just before the group, if both exist.
 */
add_task(async function test_tabGroupCollapseSelectsAdjacentTabAfter() {
  let group = gBrowser.addTabGroup("blue", "test");
  let tabInGroup = BrowserTestUtils.addTab(gBrowser, "about:blank");
  group.addTabs([tabInGroup]);
  let adjacentTabAfter = BrowserTestUtils.addTab(gBrowser, "about:blank");

  gBrowser.selectedTab = tabInGroup;

  group.collapsed = true;
  Assert.equal(
    gBrowser.selectedTab,
    adjacentTabAfter,
    "selected tab becomes adjacent tab after group on collapse"
  );

  group.collapsed = false;
  BrowserTestUtils.removeTab(tabInGroup);
  BrowserTestUtils.removeTab(adjacentTabAfter);
  group.remove();
});

/*
 * Tests that if a tab group is collapsed while the selected tab is in the group,
 * the selected tab will change to be the adjacent tab just before the group,
 * if no tabs exist after the group
 */
add_task(async function test_tabGroupCollapseSelectsAdjacentTabBefore() {
  let adjacentTabBefore = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let group = gBrowser.addTabGroup("blue", "test");
  let tabInGroup = BrowserTestUtils.addTab(gBrowser, "about:blank");
  group.addTabs([tabInGroup]);

  gBrowser.selectedTab = tabInGroup;

  group.collapsed = true;
  Assert.equal(
    gBrowser.selectedTab,
    adjacentTabBefore,
    "selected tab becomes adjacent tab after group on collapse"
  );

  group.collapsed = false;
  BrowserTestUtils.removeTab(tabInGroup);
  BrowserTestUtils.removeTab(adjacentTabBefore);
  group.remove();
});

add_task(async function test_tabGroupCollapseCreatesNewTabIfAllTabsInGroup() {
  // This test has to be run in a new window because there is currently no
  // API to remove a tab from a group, which breaks tests following this one
  // This can be removed once the group remove API is implemented
  let fgWindow = await BrowserTestUtils.openNewBrowserWindow();

  let group = fgWindow.gBrowser.addTabGroup("blue", "test");
  group.addTabs(fgWindow.gBrowser.tabs);

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

  group.collapsed = false;
  BrowserTestUtils.removeTab(fgWindow.gBrowser.tabs[1]);
  group.remove();
  await BrowserTestUtils.closeWindow(fgWindow);
});
