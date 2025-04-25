/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const {
  Management: {
    global: { getExtTabGroupIdForInternalTabGroupId },
  },
} = ChromeUtils.importESModule("resource://gre/modules/Extension.sys.mjs");

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

add_task(async function tabsGroups_get_private() {
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
