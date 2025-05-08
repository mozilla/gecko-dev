/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// This test verifies that tabGroups events do not fire for private browsing
// windows when the extension does not have access to it.
add_task(async function test_tabGroups_events_from_private_browsing() {
  let extensionWithoutAccess = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabGroups"],
    },
    background() {
      browser.tabGroups.onCreated.addListener(group => {
        browser.test.fail(`Unexpected onCreated: ${JSON.stringify(group)}`);
      });
      browser.tabGroups.onMoved.addListener(group => {
        browser.test.fail(`Unexpected onMoved: ${JSON.stringify(group)}`);
      });
      browser.tabGroups.onRemoved.addListener(group => {
        browser.test.fail(`Unexpected onRemoved: ${JSON.stringify(group)}`);
      });
      browser.tabGroups.onUpdated.addListener(group => {
        browser.test.fail(`Unexpected onUpdated: ${JSON.stringify(group)}`);
      });
      browser.test.onMessage.addListener(async (msg, groupId) => {
        browser.test.assertEq("groupId", msg, "Expected message with groupId");
        await browser.test.assertRejects(
          browser.tabGroups.get(groupId),
          `No group with id: ${groupId}`,
          "get() with groupId from private window should reject"
        );
        await browser.test.assertRejects(
          browser.tabGroups.update(groupId, {}),
          `No group with id: ${groupId}`,
          "update() with groupId from private window should reject"
        );
        await browser.test.assertRejects(
          browser.tabGroups.move(groupId, { index: 0 }),
          `No group with id: ${groupId}`,
          "move() with groupId from private window should reject"
        );
        browser.test.assertDeepEq(
          [],
          await browser.tabGroups.query({}),
          "query() does not see any tab groups from private browsing"
        );
        browser.test.sendMessage("done_part1");
      });
    },
  });

  let extensionWithAccess = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabGroups"],
    },
    incognitoOverride: "spanning",
    async background() {
      let events = [];
      // The exact contents of these events are already covered by other unit
      // tests, so here we just check whether the events are fired, as a sanity
      // check against the events in extensionWithoutAccess.
      browser.tabGroups.onCreated.addListener(() => {
        events.push("onCreated");
      });
      browser.tabGroups.onMoved.addListener(() => {
        events.push("onMoved");
      });
      browser.tabGroups.onRemoved.addListener(() => {
        events.push("onRemoved");
      });
      browser.tabGroups.onUpdated.addListener(() => {
        events.push("onUpdated");
      });
      let window = await browser.windows.create({ incognito: true });
      let tab = await browser.tabs.create({ windowId: window.id });
      const groupOptions = {
        tabIds: tab.id,
        createProperties: { windowId: window.id },
      };
      // Create a group and immediately replace with a new group, so that we
      // can observe onRemoved for the initial group.
      await browser.tabs.group(groupOptions);
      let groupId = await browser.tabs.group(groupOptions);
      await browser.tabGroups.move(groupId, { index: 0 });
      await browser.tabGroups.update(groupId, { title: "Something else" });
      browser.test.onMessage.addListener(async msg => {
        browser.test.assertEq("finish_and_cleanup", msg, "Expected message");
        browser.test.assertDeepEq(
          ["onCreated", "onCreated", "onRemoved", "onMoved", "onUpdated"],
          events,
          "Expected tabGroups events"
        );
        await browser.windows.remove(window.id);
        browser.test.sendMessage("done_part2");
      });
      browser.test.sendMessage("test_groupId", groupId);
    },
  });

  await extensionWithoutAccess.startup();
  await extensionWithAccess.startup();
  let groupId = await extensionWithAccess.awaitMessage("test_groupId");
  extensionWithoutAccess.sendMessage("groupId", groupId);
  await extensionWithoutAccess.awaitMessage("done_part1");
  extensionWithAccess.sendMessage("finish_and_cleanup");
  await extensionWithAccess.awaitMessage("done_part2");
  await extensionWithAccess.unload();
  await extensionWithoutAccess.unload();
});
