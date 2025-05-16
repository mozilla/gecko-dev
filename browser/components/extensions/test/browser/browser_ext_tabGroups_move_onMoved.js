/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const {
  Management: {
    global: { getExtTabGroupIdForInternalTabGroupId },
  },
} = ChromeUtils.importESModule("resource://gre/modules/Extension.sys.mjs");

add_task(async function tabsGroups_move_onMoved() {
  async function loadExt() {
    let ext = ExtensionTestUtils.loadExtension({
      manifest: {
        permissions: ["tabGroups"],
      },
      incognitoOverride: "spanning",
      async background() {
        browser.tabGroups.onCreated.addListener(group => {
          browser.test.sendMessage("created", group);
        });
        browser.tabGroups.onRemoved.addListener((group, removeInfo) => {
          let removed = { gid: group.id };
          if (removeInfo.isWindowClosing) {
            removed.isWindowClosing = true;
          }
          browser.test.sendMessage("removed", removed);
        });
        browser.tabGroups.onMoved.addListener(moved => {
          browser.test.sendMessage("moved", moved);
        });
        browser.test.onMessage.addListener(async (groupId, moveProps) => {
          try {
            let group = await browser.tabGroups.move(groupId, moveProps);
            browser.test.sendMessage("done", group);
          } catch (e) {
            browser.test.sendMessage("error", e.message);
          }
        });
        let [tab] = await browser.tabs.query({
          lastFocusedWindow: true,
          active: true,
        });
        browser.test.sendMessage("windowId", tab.windowId);
      },
    });
    await ext.startup();
    return ext;
  }

  let tabs = [];
  let url = "https://example.com/foo?";
  for (let i = 1; i < 10; i++) {
    tabs.push(await BrowserTestUtils.openNewForegroundTab(gBrowser, url + i));
  }

  let group = gBrowser.addTabGroup(gBrowser.tabs.slice(-2));
  is(group.tabs[1], gBrowser.tabs.at(-1), "Group's last tab is last.");

  let gid = getExtTabGroupIdForInternalTabGroupId(group.id);

  let ext = await loadExt();
  let windowId = await ext.awaitMessage("windowId");

  ext.sendMessage(gid, { index: 6 });
  await Promise.all([ext.awaitMessage("done"), ext.awaitMessage("moved")]);
  is(group.tabs[0], gBrowser.tabs[6], "Group's first tab moved to index 6.");

  ext.sendMessage(gid, { index: 3 });
  await Promise.all([ext.awaitMessage("done"), ext.awaitMessage("moved")]);
  is(group.tabs[0], gBrowser.tabs[3], "Group's first tab moved to index 3.");

  ext.sendMessage(gid, { index: 3 });
  await ext.awaitMessage("done");
  is(group.tabs[0], gBrowser.tabs[3], "Using same index 3 doesn't move.");

  ext.sendMessage(gid, { index: -1 });
  await Promise.all([ext.awaitMessage("done"), ext.awaitMessage("moved")]);
  is(group.tabs[1], gBrowser.tabs.at(-1), "Group moved to the end.");

  ext.sendMessage(gid, { index: -1 });
  await ext.awaitMessage("done");
  is(group.tabs[1], gBrowser.tabs.at(-1), "Using same index -1 doesn't move.");

  ext.sendMessage(gid, { index: 0 });
  await Promise.all([ext.awaitMessage("done"), ext.awaitMessage("moved")]);
  is(group.tabs[0], gBrowser.tabs[0], "Group moved to the beginning.");

  ext.sendMessage(gid, { index: 0 });
  await ext.awaitMessage("done");
  is(group.tabs[0], gBrowser.tabs[0], "Using same index 0 doesn't move.");

  info("Create a large second group, and remove tabs in order.");
  let group4 = gBrowser.addTabGroup(gBrowser.tabs.slice(5));
  let created4 = await ext.awaitMessage("created");
  let gid4 = getExtTabGroupIdForInternalTabGroupId(group4.id);
  is(created4.id, gid4, "Correct group 4 created event.");

  for (let tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }
  let removed1 = await ext.awaitMessage("removed");
  let removed4 = await ext.awaitMessage("removed");
  Assert.deepEqual(removed1, { gid }, "Correct group 1 removed event.");
  Assert.deepEqual(removed4, { gid: gid4 }, "Correct group 4 removed event.");

  info("Test moving a group from a non-private to a private window.");
  let win2 = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  let tab2 = await BrowserTestUtils.openNewForegroundTab(win2.gBrowser, url);
  let group2 = win2.gBrowser.addTabGroup([tab2]);
  let gid2 = getExtTabGroupIdForInternalTabGroupId(group2.id);
  let created2 = await ext.awaitMessage("created");
  is(created2.id, gid2, "Correct group 2 create event.");

  ext.sendMessage(gid2, { index: 0, windowId });
  let error = await ext.awaitMessage("error");
  is(error, "Can't move groups between private and non-private windows");
  await BrowserTestUtils.closeWindow(win2);
  let removed2 = await ext.awaitMessage("removed");
  Assert.deepEqual(
    removed2,
    { gid: gid2, isWindowClosing: true },
    "onRemoved for closed window containing group 2"
  );

  info("Test moving a group to another window.");
  let win3 = await BrowserTestUtils.openNewBrowserWindow();
  let tab3 = await BrowserTestUtils.openNewForegroundTab(win3.gBrowser, url);
  let group3 = win3.gBrowser.addTabGroup([tab3]);
  let gid3 = getExtTabGroupIdForInternalTabGroupId(group3.id);
  let created3 = await ext.awaitMessage("created");
  is(created3.id, gid3, "Correct group 3 create event.");

  ext.sendMessage(gid3, { index: 0, windowId });
  await ext.awaitMessage("done");

  let moved3 = await ext.awaitMessage("moved");
  // Chrome fires onRemoved + onCreated, but we intentionally dispatch onMoved
  // because it makes more sense - bug 1962475.
  is(moved3.id, gid3, "onMoved fired after moving to another window");

  // Add and remove a tab group, so that if onCreated/onRemoved was
  // unexpectedly fired by the move above, that we will see (unexpected) gid3
  // instead of (expected) gid4.
  let tab5 = await BrowserTestUtils.openNewForegroundTab(win3.gBrowser, url);
  let group5 = win3.gBrowser.addTabGroup([tab5]);
  let created5 = await ext.awaitMessage("created");
  let gid5 = getExtTabGroupIdForInternalTabGroupId(group5.id);
  is(created5.id, gid5, "Correct group 5 create event.");
  win3.gBrowser.removeTabGroup(group5);
  let removed5 = await ext.awaitMessage("removed");
  Assert.deepEqual(removed5, { gid: gid5 }, "Correct group 5 removed event.");

  await BrowserTestUtils.closeWindow(win3);
  let group3b = gBrowser.getTabGroupById(group3.id);
  BrowserTestUtils.removeTab(group3b.tabs[0]);
  let removed3 = await ext.awaitMessage("removed");
  Assert.deepEqual(removed3, { gid: gid3 }, "Correct group 3 removed event.");

  await ext.unload();
});

// Regression test for bug 1965057. Verify that replaceGroupWithWindow triggers
// the expected tabGroups events (tabGroups.onMoved).
add_task(async function test_replaceGroupWithWindow() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/?tab_to_group"
  );
  let group = gBrowser.addTabGroup([tab]);

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabGroups"],
    },
    async background() {
      browser.tabGroups.onMoved.addListener(group => {
        browser.test.sendMessage("moved_group", group);
      });
      browser.tabGroups.onCreated.addListener(group => {
        browser.test.fail(`Unexpected onCreated: ${JSON.stringify(group)}`);
      });
      browser.tabGroups.onRemoved.addListener(group => {
        browser.test.fail(`Unexpected onRemoved: ${JSON.stringify(group)}`);
      });
      browser.tabs.onDetached.addListener((movedTabId, detachInfo) => {
        browser.test.sendMessage("onDetached_detachInfo", detachInfo);
      });
      browser.tabs.onAttached.addListener((movedTabId, attachInfo) => {
        browser.test.sendMessage("onAttached_attachInfo", attachInfo);
      });
      browser.windows.onCreated.addListener(window => {
        browser.test.sendMessage("created_windowId", window.id);
      });
      let groups = await browser.tabGroups.query({});
      browser.test.assertEq(
        1,
        groups.length,
        `Found the one group: ${JSON.stringify(groups)}`
      );
      browser.test.sendMessage("initial_group", groups[0]);
    },
  });

  await extension.startup();
  let groupBefore = await extension.awaitMessage("initial_group");
  let oldPosition = tab._tPos;
  let groupRemoved = BrowserTestUtils.waitForEvent(group, "TabGroupRemoved");
  let newWindow = gBrowser.replaceGroupWithWindow(group);
  let newWindowId = await extension.awaitMessage("created_windowId");
  let groupAfter = await extension.awaitMessage("moved_group");
  Assert.deepEqual(
    groupAfter,
    { ...groupBefore, windowId: newWindowId },
    "Expected group after moving group with replaceGroupWithWindow"
  );
  Assert.notEqual(groupBefore.windowId, newWindowId, "windowId changed");
  Assert.deepEqual(
    await extension.awaitMessage("onDetached_detachInfo"),
    { oldWindowId: groupBefore.windowId, oldPosition },
    "Tab did indeed move to the new window"
  );
  Assert.deepEqual(
    await extension.awaitMessage("onAttached_attachInfo"),
    { newWindowId, newPosition: 0 },
    "Tab did indeed move to the new window"
  );
  await groupRemoved;
  await extension.unload();
  await BrowserTestUtils.closeWindow(newWindow);
});
