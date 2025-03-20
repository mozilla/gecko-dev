/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let resetTelemetry = async () => {
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();
};

add_task(async function test_tabGroupTelemetry() {
  await resetTelemetry();

  let tabGroupCreateTelemetry;

  let group1tab = BrowserTestUtils.addTab(gBrowser, "https://example.com");
  await BrowserTestUtils.browserLoaded(group1tab.linkedBrowser);

  let group1 = gBrowser.addTabGroup([group1tab], {
    isUserCreated: true,
    telemetryUserCreateSource: "test-source",
  });
  gBrowser.tabGroupMenu.close();

  await BrowserTestUtils.waitForCondition(() => {
    tabGroupCreateTelemetry = Glean.tabgroup.createGroup.testGetValue();
    return (
      tabGroupCreateTelemetry?.length == 1 &&
      Glean.tabgroup.tabCountInGroups.inside.testGetValue() !== null &&
      Glean.tabgroup.tabsPerActiveGroup.average.testGetValue() !== null
    );
  }, "Wait for createGroup and at least one metric from the tabCountInGroups and tabsPerActiveGroup to be set");

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

  Assert.equal(
    Glean.tabgroup.tabCountInGroups.inside.testGetValue(),
    1,
    "tabCountInGroups.inside has correct value"
  );
  Assert.equal(
    Glean.tabgroup.tabCountInGroups.outside.testGetValue(),
    1,
    "tabCountInGroups.outside has correct value"
  );
  Assert.equal(
    Glean.tabgroup.tabsPerActiveGroup.median.testGetValue(),
    1,
    "tabsPerActiveGroup.median has correct value"
  );
  Assert.equal(
    Glean.tabgroup.tabsPerActiveGroup.average.testGetValue(),
    1,
    "tabsPerActiveGroup.average has correct value"
  );
  Assert.equal(
    Glean.tabgroup.tabsPerActiveGroup.max.testGetValue(),
    1,
    "tabsPerActiveGroup.max has correct value"
  );
  Assert.equal(
    Glean.tabgroup.tabsPerActiveGroup.min.testGetValue(),
    1,
    "tabsPerActiveGroup.min has correct value"
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
    return (
      Glean.tabgroup.tabCountInGroups.inside.testGetValue() !== null &&
      Glean.tabgroup.tabsPerActiveGroup.average.testGetValue() !== null
    );
  }, "Wait for at least one metric from the tabCountInGroups and tabsPerActiveGroup to be set after adding a new tab group");

  Assert.equal(
    Glean.tabgroup.tabCountInGroups.inside.testGetValue(),
    4,
    "tabCountInGroups.inside has correct value after adding a new tab group"
  );
  Assert.equal(
    Glean.tabgroup.tabCountInGroups.outside.testGetValue(),
    1,
    "tabCountInGroups.outside has correct value after adding a new tab group"
  );
  Assert.equal(
    Glean.tabgroup.tabsPerActiveGroup.median.testGetValue(),
    2,
    "tabsPerActiveGroup.median has correct value after adding a new tab group"
  );
  Assert.equal(
    Glean.tabgroup.tabsPerActiveGroup.average.testGetValue(),
    2,
    "tabsPerActiveGroup.average has correct value after adding a new tab group"
  );
  Assert.equal(
    Glean.tabgroup.tabsPerActiveGroup.max.testGetValue(),
    3,
    "tabsPerActiveGroup.max has correct value after adding a new tab group"
  );
  Assert.equal(
    Glean.tabgroup.tabsPerActiveGroup.min.testGetValue(),
    1,
    "tabsPerActiveGroup.min has correct value after adding a new tab group"
  );

  await resetTelemetry();

  let newTabInGroup2 = BrowserTestUtils.addTab(gBrowser, "https://example.com");
  await BrowserTestUtils.browserLoaded(newTabInGroup2.linkedBrowser);

  group2.addTabs([newTabInGroup2]);

  await BrowserTestUtils.waitForCondition(() => {
    return (
      Glean.tabgroup.tabCountInGroups.inside.testGetValue() !== null &&
      Glean.tabgroup.tabsPerActiveGroup.average.testGetValue() !== null
    );
  }, "Wait for at least one metric from the tabCountInGroups and tabsPerActiveGroup to be set after modifying a tab group");

  Assert.equal(
    Glean.tabgroup.tabCountInGroups.inside.testGetValue(),
    5,
    "tabCountInGroups.inside has correct value after modifying a tab group"
  );
  Assert.equal(
    Glean.tabgroup.tabCountInGroups.outside.testGetValue(),
    1,
    "tabCountInGroups.outside has correct value after modifying a tab group"
  );
  Assert.equal(
    Glean.tabgroup.tabsPerActiveGroup.median.testGetValue(),
    2,
    "tabsPerActiveGroup.median has correct value after modifying a tab group"
  );
  Assert.equal(
    Glean.tabgroup.tabsPerActiveGroup.average.testGetValue(),
    2,
    "tabsPerActiveGroup.average has correct value after modifying a tab group"
  );
  Assert.equal(
    Glean.tabgroup.tabsPerActiveGroup.max.testGetValue(),
    4,
    "tabsPerActiveGroup.max has correct value after modifying a tab group"
  );
  Assert.equal(
    Glean.tabgroup.tabsPerActiveGroup.min.testGetValue(),
    1,
    "tabsPerActiveGroup.min has correct value after modifying a tab group"
  );

  await resetTelemetry();

  group2.collapsed = true;

  await BrowserTestUtils.waitForCondition(() => {
    return Glean.tabgroup.activeGroups.collapsed.testGetValue() !== null;
  }, "Wait for the activeGroups metric to be set after collapsing a tab group");

  Assert.equal(
    Glean.tabgroup.activeGroups.collapsed.testGetValue(),
    1,
    "activeGroups.collapsed has correct value after collapsing a tab group"
  );
  Assert.equal(
    Glean.tabgroup.activeGroups.expanded.testGetValue(),
    1,
    "activeGroups.collapsed has correct value after collapsing a tab group"
  );

  await resetTelemetry();

  await removeTabGroup(group1);
  await removeTabGroup(group2);
});
