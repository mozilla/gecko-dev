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
