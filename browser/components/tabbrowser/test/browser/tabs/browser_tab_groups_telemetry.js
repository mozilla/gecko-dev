/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { TabStateFlusher } = ChromeUtils.importESModule(
  "resource:///modules/sessionstore/TabStateFlusher.sys.mjs"
);

let resetTelemetry = async () => {
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();
};

window.gTabsPanel.init();

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

/**
 * @param {MozTabbrowserTabGroup} tabGroup
 * @returns {Promise<MozPanel>}
 *   Panel holding the tab group context menu for the requested tab group.
 */
async function openTabGroupContextMenu(tabGroup) {
  let tabgroupEditor = document.getElementById("tab-group-editor");
  let tabgroupPanel = tabgroupEditor.panel;

  let panelShown = BrowserTestUtils.waitForPopupEvent(tabgroupPanel, "shown");
  EventUtils.synthesizeMouseAtCenter(
    tabGroup.querySelector(".tab-group-label"),
    { type: "contextmenu", button: 2 },
    window
  );
  await panelShown;

  return tabgroupPanel;
}

add_task(async function test_tabGroupContextMenu_deleteTabGroup() {
  await resetTelemetry();

  let tab = BrowserTestUtils.addTab(gBrowser, "https://example.com");
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  let group = gBrowser.addTabGroup([tab]);
  // Close the automatically-opened "create tab group" menu.
  gBrowser.tabGroupMenu.close();
  let groupId = group.id;

  let menu = await openTabGroupContextMenu(group);
  let deleteTabGroupButton = menu.querySelector("#tabGroupEditor_deleteGroup");
  deleteTabGroupButton.click();

  await TestUtils.waitForCondition(
    () => !gBrowser.tabGroups.includes(group),
    "wait for group to be deleted"
  );

  let tabGroupDeleteEvents = Glean.tabgroup.delete.testGetValue();
  Assert.equal(
    tabGroupDeleteEvents.length,
    1,
    "should have recorded a tabgroup.delete event"
  );

  let [tabGroupDeleteEvent] = tabGroupDeleteEvents;
  Assert.deepEqual(
    tabGroupDeleteEvent.extra,
    {
      source: "tab_group",
      id: groupId,
    },
    "should have recorded the correct source and ID"
  );

  await resetTelemetry();
});

/**
 * @returns {Promise<PanelView>}
 */
async function openTabsMenu() {
  let viewShown = BrowserTestUtils.waitForEvent(
    window.document.getElementById("allTabsMenu-allTabsView"),
    "ViewShown"
  );
  window.document.getElementById("alltabs-button").click();
  return (await viewShown).target;
}

/**
 * @returns {Promise<void>}
 */
async function closeTabsMenu() {
  let panel = window.document
    .getElementById("allTabsMenu-allTabsView")
    .closest("panel");
  if (!panel) {
    return;
  }
  let hidden = BrowserTestUtils.waitForPopupEvent(panel, "hidden");
  panel.hidePopup();
  await hidden;
}

/**
 * @param {XULToolbarButton} triggerNode
 * @param {string} contextMenuId
 * @returns {Promise<XULMenuElement|XULPopupElement>}
 */
async function getContextMenu(triggerNode, contextMenuId) {
  let win = triggerNode.ownerGlobal;
  triggerNode.scrollIntoView();
  const contextMenu = win.document.getElementById(contextMenuId);
  const contextMenuShown = BrowserTestUtils.waitForPopupEvent(
    contextMenu,
    "shown"
  );

  EventUtils.synthesizeMouseAtCenter(
    triggerNode,
    { type: "contextmenu", button: 2 },
    win
  );
  await contextMenuShown;
  return contextMenu;
}

/**
 * @param {XULMenuElement|XULPopupElement} contextMenu
 * @returns {Promise<void>}
 */
async function closeContextMenu(contextMenu) {
  let menuHidden = BrowserTestUtils.waitForPopupEvent(contextMenu, "hidden");
  contextMenu.hidePopup();
  await menuHidden;
}

/**
 * Returns a new basic, unnamed tab group that is fully loaded in the browser
 * and in session state.
 *
 * @returns {Promise<MozTabbrowserTabGroup>}
 */
async function makeTabGroup() {
  let tab = BrowserTestUtils.addTab(gBrowser, "https://example.com");
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await TabStateFlusher.flush(tab.linkedBrowser);

  let group = gBrowser.addTabGroup([tab]);
  // Close the automatically-opened "create tab group" menu.
  gBrowser.tabGroupMenu.close();
  return group;
}

add_task(async function test_tabOverflowContextMenu_deleteOpenTabGroup() {
  await resetTelemetry();

  info("set up an open tab group to be deleted");
  let openGroup = await makeTabGroup();
  let openGroupId = openGroup.id;

  info("delete the open tab group");
  let allTabsMenu = await openTabsMenu();
  let tabGroupButton = allTabsMenu.querySelector(
    `#allTabsMenu-groupsView [data-tab-group-id="${openGroupId}"]`
  );

  let menu = await getContextMenu(
    tabGroupButton,
    "open-tab-group-context-menu"
  );

  menu.querySelector("#open-tab-group-context-menu_delete").click();
  await closeContextMenu(menu);
  await closeTabsMenu();

  await TestUtils.waitForCondition(
    () => !gBrowser.tabGroups.includes(openGroup),
    "wait for group to be deleted"
  );

  let tabGroupDeleteEvents = Glean.tabgroup.delete.testGetValue();
  Assert.equal(
    tabGroupDeleteEvents.length,
    1,
    "should have recorded one tabgroup.delete event"
  );

  let [openTabGroupDeleteEvent] = tabGroupDeleteEvents;

  Assert.deepEqual(
    openTabGroupDeleteEvent.extra,
    {
      source: "tab_overflow",
      id: openGroupId,
    },
    "should have recorded the correct source and ID for the open tab group"
  );

  await resetTelemetry();
});
