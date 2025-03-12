/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let resetTelemetry = async () => {
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();
};

add_task(async function test_tabGroupTelemetry() {
  await resetTelemetry();

  let tabGroupCreateTelemetry,
    tabGroupModifyTelemetry,
    tabGroupCollapseTelemetry;

  let group1tab = BrowserTestUtils.addTab(gBrowser, "https://example.com");
  await BrowserTestUtils.browserLoaded(group1tab.linkedBrowser);

  let group1 = gBrowser.addTabGroup([group1tab], {
    isUserCreated: true,
    telemetryUserCreateSource: "test-source",
  });
  gBrowser.tabGroupMenu.close();

  await BrowserTestUtils.waitForCondition(() => {
    tabGroupCreateTelemetry =
      Glean.browserEngagement.tabGroupCreate.testGetValue();
    tabGroupModifyTelemetry =
      Glean.browserEngagement.tabGroupModify.testGetValue();
    return (
      tabGroupCreateTelemetry?.length == 1 &&
      tabGroupModifyTelemetry?.length == 1
    );
  }, "Wait for tabGroupCreate and tabGroupModify events after creating a single tab group");

  Assert.deepEqual(
    tabGroupCreateTelemetry[0].extra,
    {
      id: group1.id,
      layout: "horizontal",
      source: "test-source",
      tabs: "1",
    },
    "tabGroupCreate event extra_keys has correct values after tab group create"
  );
  Assert.deepEqual(
    tabGroupModifyTelemetry[0].extra,
    {
      tabs_per_active_group_min: "1",
      tabs_per_active_group_max: "1",
      tabs_inside_groups: "1",
      tabs_per_active_group_median: "1",
      tabs_outside_groups: "1",
      tabs_per_active_group_average: "1",
    },
    "tabGroupModify event extra_keys has correct values after tab group create"
  );

  await resetTelemetry();

  let group2Tabs = [
    BrowserTestUtils.addTab(gBrowser, "https://example.com"),
    BrowserTestUtils.addTab(gBrowser, "https://example.com"),
    BrowserTestUtils.addTab(gBrowser, "https://example.com"),
  ];
  await Promise.all(
    group2Tabs.map(t => BrowserTestUtils.browserLoaded(t.linkedBrowser))
  );

  let group2 = gBrowser.addTabGroup(group2Tabs, {
    isUserCreated: true,
    telemetryUserCreateSource: "test-source",
  });
  gBrowser.tabGroupMenu.close();

  await BrowserTestUtils.waitForCondition(() => {
    tabGroupModifyTelemetry =
      Glean.browserEngagement.tabGroupModify.testGetValue();
    return tabGroupModifyTelemetry?.length == 1;
  }, "Wait for tabGroupModify event after adding a new tab group");

  Assert.deepEqual(
    tabGroupModifyTelemetry[0].extra,
    {
      tabs_per_active_group_max: "3",
      tabs_per_active_group_min: "1",
      tabs_per_active_group_median: "2",
      tabs_per_active_group_average: "2",
      tabs_inside_groups: "4",
      tabs_outside_groups: "1",
    },
    "tabGroupModify event extra_keys has correct values after adding a new tab group"
  );
  await resetTelemetry();

  let newTabInGroup2 = BrowserTestUtils.addTab(gBrowser, "https://example.com");
  await BrowserTestUtils.browserLoaded(newTabInGroup2.linkedBrowser);

  group2.addTabs([newTabInGroup2]);

  await BrowserTestUtils.waitForCondition(() => {
    tabGroupModifyTelemetry =
      Glean.browserEngagement.tabGroupModify.testGetValue();
    return tabGroupModifyTelemetry?.length == 1;
  }, "Wait for tabGroupModify event after modifying a tab group");

  Assert.deepEqual(
    tabGroupModifyTelemetry[0].extra,
    {
      tabs_per_active_group_max: "4",
      tabs_per_active_group_min: "1",
      tabs_per_active_group_median: "2.5",
      tabs_per_active_group_average: "2.5",
      tabs_inside_groups: "5",
      tabs_outside_groups: "1",
    },
    "tabGroupModify event extra_keys has correct values after changing the number of tabs in groups"
  );
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();

  group2.collapsed = true;

  await BrowserTestUtils.waitForCondition(() => {
    tabGroupCollapseTelemetry =
      Glean.browserEngagement.tabGroupExpandOrCollapse.testGetValue();
    return tabGroupCollapseTelemetry?.length;
  }, "Wait for tabGroupExpandOrCollapseEvent after tab group collapse");

  Assert.deepEqual(
    tabGroupCollapseTelemetry[0].extra,
    {
      total_collapsed: "1",
      total_expanded: "1",
    },
    "tabGroupExpandOrCollapse event extra_keys has correct values"
  );
  await resetTelemetry();

  await removeTabGroup(group1);
  await removeTabGroup(group2);
});
