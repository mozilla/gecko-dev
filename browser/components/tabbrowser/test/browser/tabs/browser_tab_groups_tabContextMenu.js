/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Context menu tests
// ---

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

  let contextMenuHidden = BrowserTestUtils.waitForPopupEvent(
    tabContextMenu,
    "hidden"
  );
  await callback(moveTabToNewGroupItem, moveTabToGroupItem, ungroupTabItem);
  tabContextMenu.hidePopup();
  return contextMenuHidden;
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

    let tabGrouped = BrowserTestUtils.waitForEvent(group, "TabGrouped");
    let otherWindowActivated = BrowserTestUtils.waitForEvent(
      otherWindow,
      "activate"
    );
    await withTabMenu(tab, async (_, moveTabToGroupItem) => {
      moveTabToGroupItem.querySelector(`[tab-group-id="${group.id}"]`).click();
    });
    await Promise.allSettled([tabGrouped, otherWindowActivated]);
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
