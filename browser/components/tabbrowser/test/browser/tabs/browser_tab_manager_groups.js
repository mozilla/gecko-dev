/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.groups.enabled", true]],
  });

  const tabGroups = SessionStore.getSavedTabGroups();
  tabGroups.forEach(tabGroup => SessionStore.forgetSavedTabGroup(tabGroup.id));

  window.gTabsPanel.init();
});

async function openTabsMenu(win = window) {
  return new Promise(resolve => {
    BrowserTestUtils.waitForEvent(
      win.document.getElementById("allTabsMenu-allTabsView"),
      "ViewShown"
    ).then(event => resolve(event.target));
    win.document.getElementById("alltabs-button").click();
  });
}

async function closeTabsMenu(win = window) {
  return new Promise(resolve => {
    let panel = win.document
      .getElementById("allTabsMenu-allTabsView")
      .closest("panel");
    BrowserTestUtils.waitForPopupEvent(panel, "hidden").then(event =>
      resolve(event.target)
    );
    panel.hidePopup();
  });
}

/**
 * Tests that grouped tabs in alltabsmenu are prepended by
 * a group indicator
 */
add_task(async function test_allTabsView() {
  let tabs = [];
  for (let i = 1; i <= 5; i++) {
    tabs.push(
      await addTab(`data:text/plain,tab${i}`, {
        skipAnimation: true,
      })
    );
  }
  gBrowser.addTabGroup([tabs[0], tabs[1]], {
    label: "Test Group",
  });
  gBrowser.addTabGroup([tabs[2], tabs[3]]);

  let allTabsMenu = await openTabsMenu();

  let tabButtons = allTabsMenu.querySelectorAll(
    "#allTabsMenu-allTabsView-tabs .all-tabs-button"
  );
  let expectedLabels = [
    "New Tab",
    "data:text/plain,tab5",
    "Test Group",
    "data:text/plain,tab1",
    "data:text/plain,tab2",
    "Unnamed Group",
    "data:text/plain,tab3",
    "data:text/plain,tab4",
  ];
  tabButtons.forEach((button, i) => {
    Assert.equal(
      button.label,
      expectedLabels[i],
      `Expected: ${expectedLabels[i]}`
    );
  });

  await closeTabsMenu();
  for (let tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }
});

/**
 * Tests that groups appear in the supplementary group menu
 * when they are saved (and closed,) or open in another window.
 * Clicking an open group in this menu focuses it,
 * and clicking on a saved group restores it.
 */
add_task(async function test_tabGroupsView() {
  let tabs = [];
  for (let i = 1; i <= 5; i++) {
    tabs.push(
      await addTab(`data:text/plain,tab${i}`, {
        skipAnimation: true,
      })
    );
  }
  let group1 = gBrowser.addTabGroup([tabs[0], tabs[1]], {
    id: "test-saved-group",
    label: "Test Saved Group",
  });
  let group2 = gBrowser.addTabGroup([tabs[2], tabs[3]], {
    label: "Test Open Group",
  });

  let newWindow = await BrowserTestUtils.openNewBrowserWindow();
  newWindow.gTabsPanel.init();

  let allTabsMenu = await openTabsMenu(newWindow);
  Assert.equal(
    allTabsMenu.querySelectorAll("#allTabsMenu-groupsView .all-tabs-button")
      .length,
    2,
    "Both groups shown in groups list"
  );
  Assert.ok(
    !allTabsMenu.querySelector(
      "#allTabsMenu-groupsView .all-tabs-button.all-tabs-group-saved-group"
    ),
    "Neither group is shown as saved"
  );

  await closeTabsMenu(newWindow);

  group1.save();
  await removeTabGroup(group1);

  Assert.ok(!gBrowser.getTabGroupById("test-saved-group"), "Group 1 removed");

  allTabsMenu = await openTabsMenu(newWindow);
  Assert.equal(
    allTabsMenu.querySelectorAll("#allTabsMenu-groupsView .all-tabs-button")
      .length,
    2,
    "Both groups shown in groups list"
  );
  let savedGroupButton = allTabsMenu.querySelector(
    "#allTabsMenu-groupsView .all-tabs-button.all-tabs-group-saved-group"
  );
  Assert.equal(
    savedGroupButton.label,
    "Test Saved Group",
    "Saved group appears as saved"
  );

  // Clicking on an open group should select that group in the origin window
  let openGroupButton = allTabsMenu.querySelector(
    "#allTabsMenu-groupsView .all-tabs-button:not(.all-tabs-group-saved-group)"
  );
  openGroupButton.click();
  Assert.equal(
    gBrowser.selectedTab.group.id,
    group2.id,
    "Tab in group 2 is selected"
  );

  await BrowserTestUtils.closeWindow(newWindow, { animate: false });

  // Clicking on a saved group should restore the group to the current window
  allTabsMenu = await openTabsMenu();
  savedGroupButton = allTabsMenu.querySelector(
    "#allTabsMenu-groupsView .all-tabs-button.all-tabs-group-saved-group"
  );
  savedGroupButton.click();
  group1 = gBrowser.getTabGroupById("test-saved-group");
  Assert.ok(group1, "Group 1 has been restored");
  allTabsMenu = await openTabsMenu();
  Assert.ok(
    !allTabsMenu.querySelector("#allTabsMenu-groupsView .all-tabs-button"),
    "Groups list is now empty for this window"
  );

  await closeTabsMenu();
  for (let tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }
  await removeTabGroup(group1);
});
