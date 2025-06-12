/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ASRouterTriggerListeners } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/ASRouterTriggerListeners.sys.mjs"
);
const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
const { TabStateFlusher } = ChromeUtils.importESModule(
  "resource:///modules/sessionstore/TabStateFlusher.sys.mjs"
);

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
