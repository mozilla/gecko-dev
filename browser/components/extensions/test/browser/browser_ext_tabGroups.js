/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const {
  Management: {
    global: { getExtTabGroupIdForInternalTabGroupId },
  },
} = ChromeUtils.importESModule("resource://gre/modules/Extension.sys.mjs");

add_task(async function tabsGroups_get_private() {
  async function loadExt(allowPrivate) {
    let ext = ExtensionTestUtils.loadExtension({
      manifest: {
        permissions: ["tabGroups"],
      },
      incognitoOverride: allowPrivate ? "spanning" : undefined,
      background() {
        browser.test.onMessage.addListener(async groupId => {
          try {
            let group = await browser.tabGroups.get(groupId);

            let color = browser.tabGroups.Color[group.color.toUpperCase()];
            browser.test.assertEq(group.color, color, "A known colour.");

            browser.test.sendMessage("group", group);
          } catch (e) {
            browser.test.sendMessage("error", e.message);
          }
        });
      },
    });
    await ext.startup();
    return ext;
  }

  let url = "https://example.com/?";
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  let group1 = gBrowser.addTabGroup([tab1]);
  let gid1 = getExtTabGroupIdForInternalTabGroupId(group1.id);

  let win2 = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  let tab2 = await BrowserTestUtils.openNewForegroundTab(win2.gBrowser, url);
  let group2 = win2.gBrowser.addTabGroup([tab2]);
  let gid2 = getExtTabGroupIdForInternalTabGroupId(group2.id);

  function compare(group, ext, info) {
    is(group.collapsed, ext.collapsed, `Collapsed ok - ${info}`);
    is(group.name, ext.title, `Title ok - ${info}`);
    is(group.color.replace(/^gray$/, "grey"), ext.color, `Color ok - ${info}`);
  }

  info("Testing extension without private browsing access.");
  let ext1 = await loadExt(false);

  ext1.sendMessage(gid1);
  let g1 = await ext1.awaitMessage("group");
  compare(group1, g1, "Ext 1 / group 1.");

  ext1.sendMessage(gid2);
  let e2 = await ext1.awaitMessage("error");
  is(e2, `No group with id: ${gid2}`, "Expected error.");
  await ext1.unload();

  info("Testing extension with private browsing access.");
  let ext2 = await loadExt(true);

  ext2.sendMessage(gid1);
  g1 = await ext2.awaitMessage("group");
  compare(group1, g1, "Ext 2 / group 1.");

  ext2.sendMessage(gid1);
  let g2 = await ext2.awaitMessage("group");
  compare(group1, g2, "Ext 2 / group 2.");

  await ext2.unload();
  BrowserTestUtils.removeTab(tab1);
  await BrowserTestUtils.closeWindow(win2);
});

add_task(async function tabsGroups_update_onUpdated() {
  let url = "https://example.com/?";
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, url + 1);
  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, url + 2);

  let group1 = gBrowser.addTabGroup([tab1]);
  let group2 = gBrowser.addTabGroup([tab2]);
  gBrowser.selectedTab = tab1;

  info("Setup initial group colour for consistency.");
  group1.color = "red";
  group2.color = "blue";

  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabGroups"],
    },
    async background() {
      let [tab] = await browser.tabs.query({
        lastFocusedWindow: true,
        active: true,
      });

      let group = await browser.tabGroups.get(tab.groupId);
      browser.test.assertEq(tab.groupId, group.id, "Group id matches.");
      browser.test.assertEq(tab.windowId, group.windowId, "Window id ok.");

      browser.tabGroups.onUpdated.addListener(async updated => {
        let group = await browser.tabGroups.get(updated.id);
        browser.test.assertDeepEq(updated, group, "It's the same group.");
        browser.test.sendMessage("updated", updated);
      });

      browser.test.assertThrows(
        () => browser.tabGroups.update(1e9, { color: "magenta" }),
        /Invalid enumeration value "magenta"/,
        "Magenta is not a real color."
      );

      await browser.test.assertRejects(
        browser.tabGroups.update(1e9, { title: "blah" }),
        "No group with id: 1000000000",
        "Invalid group id rejects."
      );

      browser.test.onMessage.addListener((_msg, update) => {
        browser.tabGroups.update(group.id, update);
      });
      browser.test.sendMessage("group", group);
    },
  });
  await ext.startup();

  function compare(first, second, info) {
    let { color, title, collapsed } = first;

    // XUL element has a .name, and uses "gray".
    if (XULElement.isInstance(first)) {
      color = first.color.replace(/^gray$/, "grey");
      title = first.name;
    }

    is(collapsed, second.collapsed, `Collapsed ok - ${info}`);
    is(color, second.color, `Color ok - ${info}`);
    is(title, second.title, `Title ok - ${info}`);
  }

  let first = await ext.awaitMessage("group");
  compare(group1, first, "Initial group.");

  info("Updating group1 using extension api.");
  ext.sendMessage("update", { collapsed: true });
  first.collapsed = true;

  let updated = await ext.awaitMessage("updated");
  compare(first, updated, "After collapsing 1.");

  ext.sendMessage("update", { collapsed: false });
  first.collapsed = false;

  updated = await ext.awaitMessage("updated");
  compare(first, updated, "After expanding 1.");

  ext.sendMessage("update", { color: "grey" });
  first.color = "grey";

  updated = await ext.awaitMessage("updated");
  compare(first, updated, "After coloring 1.");

  ext.sendMessage("update", { title: "First" });
  first.title = "First";

  updated = await ext.awaitMessage("updated");
  compare(first, updated, "After naming 1.");

  info("Creating a new group does not trigger onUpdated event.");
  let tab3 = await BrowserTestUtils.openNewForegroundTab(gBrowser, url + 3);
  gBrowser.addTabGroup([tab3]);

  info("Closing tab1, and thus its group, does not trigger onUpdated.");
  BrowserTestUtils.removeTab(tab1);

  info("Updating group2 directly from outside of the extension.");
  group2.collapsed = true;
  let second = await ext.awaitMessage("updated");
  compare(group2, second, "After collapsing 2.");

  group2.color = "pink";
  second = await ext.awaitMessage("updated");
  compare(group2, second, "After coloring 2.");

  group2.name = "Second";
  second = await ext.awaitMessage("updated");
  compare(group2, second, "After naming 2.");

  isnot(first.id, second.id, "Groups have different ids.");
  is(first.windowId, second.windowId, "Groups in the same window.");

  await ext.unload();
  BrowserTestUtils.removeTab(tab3);
  BrowserTestUtils.removeTab(tab2);
});

add_task(async function tabsGroups_move_onMoved() {
  async function loadExt() {
    let ext = ExtensionTestUtils.loadExtension({
      manifest: {
        permissions: ["tabGroups"],
      },
      incognitoOverride: "spanning",
      async background() {
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

  info("Create a large second group, and try to move in the middle of it.");
  gBrowser.addTabGroup(gBrowser.tabs.slice(5));

  ext.sendMessage(gid, { index: 8 });
  await Promise.all([ext.awaitMessage("done"), ext.awaitMessage("moved")]);
  is(group.tabs[1], gBrowser.tabs[4], "Moved before the whole group instead.");

  for (let tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }

  let win2 = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  let tab2 = await BrowserTestUtils.openNewForegroundTab(win2.gBrowser, url);
  let group2 = win2.gBrowser.addTabGroup([tab2]);
  let gid2 = getExtTabGroupIdForInternalTabGroupId(group2.id);

  ext.sendMessage(gid2, { index: 0, windowId });
  let error = await ext.awaitMessage("error");
  is(error, "Can't move groups between private and non-private windows");
  await BrowserTestUtils.closeWindow(win2);

  await ext.unload();
});
