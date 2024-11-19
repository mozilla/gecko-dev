/** @typedef {[MozTabbrowserTab, number]} TabWithIndex */

/**
 * @param {TabWithIndex[]} tabsWithIndexes
 */
function assertTabIndexesMatch(tabsWithIndexes) {
  tabsWithIndexes.forEach(([tab, expectedTabIndex], tabIndex) => {
    Assert.equal(
      tab._tPos,
      expectedTabIndex,
      `tab${tabIndex} should be at index ${expectedTabIndex}`
    );
  });
}

add_task(async function test_moveTabForward() {
  let tab1 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab2 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab3 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  gBrowser.selectedTab = tab1;

  info("validate the starting order of the tabs");
  assertTabIndexesMatch([
    [tab1, 1],
    [tab2, 2],
    [tab3, 3],
  ]);

  gBrowser.moveTabForward();
  info("tab1 should move past tab2");
  assertTabIndexesMatch([
    [tab1, 2],
    [tab2, 1],
    [tab3, 3],
  ]);

  gBrowser.moveTabForward();
  info("tab1 should move past tab3");
  assertTabIndexesMatch([
    [tab1, 3],
    [tab2, 1],
    [tab3, 2],
  ]);

  gBrowser.moveTabForward();
  info("validate that a tab at the end of the tab strip does not move");
  assertTabIndexesMatch([
    [tab1, 3],
    [tab2, 1],
    [tab3, 2],
  ]);

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);
});

add_task(async function test_moveTabForwardPinnedTabs() {
  let tab1 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab2 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab3 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab4 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  gBrowser.pinTab(tab1);
  gBrowser.pinTab(tab2);

  info("validate the starting order of the tabs");
  assertTabIndexesMatch([
    [tab1, 0],
    [tab2, 1],
    [tab3, 3],
    [tab4, 4],
  ]);

  gBrowser.selectedTab = tab1;

  gBrowser.moveTabForward();
  info("selected tab becomes second pinned tab");
  assertTabIndexesMatch([
    [tab1, 1],
    [tab2, 0],
    [tab3, 3],
    [tab4, 4],
  ]);

  gBrowser.moveTabForward();
  info(
    "selected tab should not move forward because it is the last pinned tab"
  );
  assertTabIndexesMatch([
    [tab1, 1],
    [tab2, 0],
    [tab3, 3],
    [tab4, 4],
  ]);

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);
  BrowserTestUtils.removeTab(tab4);
});

add_task(async function test_moveTabForwardTabGroups() {
  let tab1 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab2 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab3 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab4 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab5 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab6 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let group1 = gBrowser.addTabGroup([tab3], { insertBefore: tab3 });
  let group2 = gBrowser.addTabGroup([tab4], { insertBefore: tab4 });
  gBrowser.selectedTab = tab1;

  info("validate the starting order of the tabs");
  assertTabIndexesMatch([
    [tab1, 1],
    [tab2, 2],
    [tab3, 3],
    [tab4, 4],
    [tab5, 5],
    [tab6, 6],
  ]);
  Assert.ok(!tab1.group, "tab1 should not be in a tab group");

  gBrowser.moveTabForward();
  info("first two tabs should switch places");
  assertTabIndexesMatch([
    [tab1, 2],
    [tab2, 1],
    [tab3, 3],
    [tab4, 4],
    [tab5, 5],
    [tab6, 6],
  ]);
  Assert.ok(!tab1.group, "tab1 should not be in a tab group");

  gBrowser.moveTabForward();
  info("selected tab should move into the start of group1");
  assertTabIndexesMatch([
    [tab1, 2],
    [tab2, 1],
    [tab3, 3],
    [tab4, 4],
    [tab5, 5],
    [tab6, 6],
  ]);
  Assert.equal(tab1.group, group1, "tab1 should be in group1");

  gBrowser.moveTabForward();
  info("selected tab should move to the end of group1");
  assertTabIndexesMatch([
    [tab1, 3],
    [tab2, 1],
    [tab3, 2],
    [tab4, 4],
    [tab5, 5],
    [tab6, 6],
  ]);
  Assert.equal(tab1.group, group1, "tab1 should still be in group1");

  gBrowser.moveTabForward();
  info("selected tab should become a standalone tab between group1 and group2");
  assertTabIndexesMatch([
    [tab1, 3],
    [tab2, 1],
    [tab3, 2],
    [tab4, 4],
    [tab5, 5],
    [tab6, 6],
  ]);
  Assert.ok(!tab1.group, "tab1 should no longer be in a group");

  gBrowser.moveTabForward();
  info("selected tab should move into the start of group2");
  assertTabIndexesMatch([
    [tab1, 3],
    [tab2, 1],
    [tab3, 2],
    [tab4, 4],
    [tab5, 5],
    [tab6, 6],
  ]);
  Assert.equal(tab1.group, group2, "tab1 should now be in group2");

  gBrowser.moveTabForward();
  info("selected tab should move to the end of group2");
  assertTabIndexesMatch([
    [tab1, 4],
    [tab2, 1],
    [tab3, 2],
    [tab4, 3],
    [tab5, 5],
    [tab6, 6],
  ]);
  Assert.equal(tab1.group, group2, "tab1 should still be in group2");

  gBrowser.moveTabForward();
  info("selected tab should become a standalone tab after group2");
  assertTabIndexesMatch([
    [tab1, 4],
    [tab2, 1],
    [tab3, 2],
    [tab4, 3],
    [tab5, 5],
    [tab6, 6],
  ]);
  Assert.ok(!tab1.group, "tab1 should no longer be in a group");

  await removeTabGroup(group1);
  await removeTabGroup(group2);
  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab5);
  BrowserTestUtils.removeTab(tab6);
});

/* TODO Bug 1927844 for moving the active tab around collapsed tab groups */

add_task(async function test_moveTabForwardCollapsedTabGroups() {
  let tab1 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab2 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab3 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab4 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab5 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let group1 = gBrowser.addTabGroup([tab2, tab3], { insertBefore: tab2 });
  let group2 = gBrowser.addTabGroup([tab4, tab5], { insertBefore: tab4 });
  gBrowser.selectedTab = tab1;
  group1.collapsed = true;
  group2.collapsed = true;

  info("validate the starting order of the tabs");
  assertTabIndexesMatch([
    [tab1, 1],
    [tab2, 2],
    [tab3, 3],
    [tab4, 4],
    [tab5, 5],
  ]);
  Assert.ok(!tab1.group, "tab1 should not be in a tab group");

  gBrowser.moveTabForward();
  info("active tab should skip over a collapsed tab group");
  assertTabIndexesMatch([
    [tab1, 3],
    [tab2, 1],
    [tab3, 2],
    [tab4, 4],
    [tab5, 5],
  ]);
  Assert.ok(!tab1.group, "tab1 should not be in a tab group");

  gBrowser.moveTabForward();
  info("active tab should skip over another collapsed tab group");
  assertTabIndexesMatch([
    [tab1, 5],
    [tab2, 1],
    [tab3, 2],
    [tab4, 3],
    [tab5, 4],
  ]);
  Assert.ok(!tab1.group, "tab1 should still not be in a tab group");

  await removeTabGroup(group1);
  await removeTabGroup(group2);
  BrowserTestUtils.removeTab(tab1);
});

add_task(async function test_moveTabBackward() {
  let tab1 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab2 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab3 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  gBrowser.selectedTab = tab3;

  info("validate the starting order of the tabs");
  assertTabIndexesMatch([
    [tab1, 1],
    [tab2, 2],
    [tab3, 3],
  ]);

  gBrowser.moveTabBackward();
  info("tab3 should move past tab2");
  assertTabIndexesMatch([
    [tab1, 1],
    [tab2, 3],
    [tab3, 2],
  ]);

  gBrowser.moveTabBackward();
  info("tab3 should move past tab1");
  assertTabIndexesMatch([
    [tab1, 2],
    [tab2, 3],
    [tab3, 1],
  ]);

  gBrowser.moveTabBackward();
  info("tab3 should move to the start of the tab bar");
  assertTabIndexesMatch([
    [tab1, 2],
    [tab2, 3],
    [tab3, 0],
  ]);

  gBrowser.moveTabBackward();

  info("validate that a tab at the start of the tab strip does not move");
  assertTabIndexesMatch([
    [tab1, 2],
    [tab2, 3],
    [tab3, 0],
  ]);

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);
});

add_task(async function test_moveTabBackwardPinnedTabs() {
  let tab1 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab2 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab3 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab4 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  gBrowser.pinTab(tab1);
  gBrowser.pinTab(tab2);

  info("validate the starting order of the tabs");
  assertTabIndexesMatch([
    [tab1, 0],
    [tab2, 1],
    [tab3, 3],
    [tab4, 4],
  ]);

  gBrowser.selectedTab = tab2;

  gBrowser.moveTabBackward();
  info("selected tab becomes first pinned tab");
  assertTabIndexesMatch([
    [tab1, 1],
    [tab2, 0],
    [tab3, 3],
    [tab4, 4],
  ]);

  gBrowser.moveTabBackward();
  info(
    "selected tab should not move backward because it is the last pinned tab"
  );
  assertTabIndexesMatch([
    [tab1, 1],
    [tab2, 0],
    [tab3, 3],
    [tab4, 4],
  ]);

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);
  BrowserTestUtils.removeTab(tab4);
});

add_task(async function test_moveTabBackwardTabGroups() {
  let tab1 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab2 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab3 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab4 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab5 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab6 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let group1 = gBrowser.addTabGroup([tab3], { insertBefore: tab3 });
  let group2 = gBrowser.addTabGroup([tab4], { insertBefore: tab4 });
  gBrowser.selectedTab = tab6;

  info("validate the starting order of the tabs");
  assertTabIndexesMatch([
    [tab1, 1],
    [tab2, 2],
    [tab3, 3],
    [tab4, 4],
    [tab5, 5],
    [tab6, 6],
  ]);
  Assert.ok(!tab6.group, "tab6 should not be in a tab group");

  gBrowser.moveTabBackward();
  info("last two tabs should switch places");
  assertTabIndexesMatch([
    [tab1, 1],
    [tab2, 2],
    [tab3, 3],
    [tab4, 4],
    [tab5, 6],
    [tab6, 5],
  ]);
  Assert.ok(!tab6.group, "tab6 should not be in a tab group");

  gBrowser.moveTabBackward();
  info("selected tab should move into the end of group2");
  assertTabIndexesMatch([
    [tab1, 1],
    [tab2, 2],
    [tab3, 3],
    [tab4, 4],
    [tab5, 6],
    [tab6, 5],
  ]);
  Assert.equal(tab6.group, group2, "tab6 should be in group2");

  gBrowser.moveTabBackward();
  info("selected tab should move to the start of group2");
  assertTabIndexesMatch([
    [tab1, 1],
    [tab2, 2],
    [tab3, 3],
    [tab4, 5],
    [tab5, 6],
    [tab6, 4],
  ]);
  Assert.equal(tab6.group, group2, "tab6 should still be in group2");

  gBrowser.moveTabBackward();
  info("selected tab should become a standalone tab between group1 and group2");
  assertTabIndexesMatch([
    [tab1, 1],
    [tab2, 2],
    [tab3, 3],
    [tab4, 5],
    [tab5, 6],
    [tab6, 4],
  ]);
  Assert.ok(!tab6.group, "tab6 should no longer be in a group");

  gBrowser.moveTabBackward();
  info("selected tab should move into the end of group1");
  assertTabIndexesMatch([
    [tab1, 1],
    [tab2, 2],
    [tab3, 3],
    [tab4, 5],
    [tab5, 6],
    [tab6, 4],
  ]);
  Assert.equal(tab6.group, group1, "tab6 should now be in group1");

  gBrowser.moveTabBackward();
  info("selected tab should move to the start of group1");
  assertTabIndexesMatch([
    [tab1, 1],
    [tab2, 2],
    [tab3, 4],
    [tab4, 5],
    [tab5, 6],
    [tab6, 3],
  ]);
  Assert.equal(tab6.group, group1, "tab6 should still be in group2");

  gBrowser.moveTabBackward();
  info("selected tab should become a standalone tab before group1");
  assertTabIndexesMatch([
    [tab1, 1],
    [tab2, 2],
    [tab3, 4],
    [tab4, 5],
    [tab5, 6],
    [tab6, 3],
  ]);
  Assert.ok(!tab6.group, "tab6 should no longer be in a group");

  await removeTabGroup(group1);
  await removeTabGroup(group2);
  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab5);
  BrowserTestUtils.removeTab(tab6);
});

/* TODO Bug 1927844 for moving the active tab around collapsed tab groups */

add_task(async function test_moveTabBackwardCollapsedTabGroups() {
  let tab1 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab2 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab3 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab4 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let tab5 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let group1 = gBrowser.addTabGroup([tab1, tab2], { insertBefore: tab1 });
  let group2 = gBrowser.addTabGroup([tab3, tab4], { insertBefore: tab3 });
  gBrowser.selectedTab = tab5;
  group1.collapsed = true;
  group2.collapsed = true;

  info("validate the starting order of the tabs");
  assertTabIndexesMatch([
    [tab1, 1],
    [tab2, 2],
    [tab3, 3],
    [tab4, 4],
    [tab5, 5],
  ]);
  Assert.ok(!tab5.group, "tab5 should not be in a tab group");

  gBrowser.moveTabBackward();
  info("active tab should skip over a collapsed tab group");
  assertTabIndexesMatch([
    [tab1, 1],
    [tab2, 2],
    [tab3, 4],
    [tab4, 5],
    [tab5, 3],
  ]);
  Assert.ok(!tab5.group, "tab5 should not be in a tab group");

  gBrowser.moveTabBackward();
  info("active tab should skip over another collapsed tab group");
  assertTabIndexesMatch([
    [tab1, 2],
    [tab2, 3],
    [tab3, 4],
    [tab4, 5],
    [tab5, 1],
  ]);
  Assert.ok(!tab5.group, "tab5 should still not be in a tab group");

  await removeTabGroup(group1);
  await removeTabGroup(group2);
  BrowserTestUtils.removeTab(tab5);
});
