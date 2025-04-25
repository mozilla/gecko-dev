/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// TODO bug 1938594: moving a created tab to another window sometimes triggers
// this error. See https://bugzilla.mozilla.org/show_bug.cgi?id=1938594#c1
PromiseTestUtils.allowMatchingRejectionsGlobally(
  /Unexpected undefined tabState for onMoveToNewWindow/
);

// There is special logic for adopting tab groups at the end of a tab strip.
// This test tests the behavior at the penultimate tab. For extra coverage,
// this test uses extension APIs only to create windows, tabs, groups.
// It also checks tabs.onMoved, tabs.onAttached and tabs.onDetached.
add_task(async function tabGroups_move_to_other_window() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabGroups"],
    },
    async background() {
      // Setup: The tab that we are going to group.
      const { id: tabId, windowId: oldWinId } = await browser.tabs.create({});
      browser.test.log(`Tab ID: ${tabId}, old win ID: ${oldWinId}`);
      const groupId = await browser.tabs.group({ tabIds: tabId });
      // Setup: Create window with two tabs.
      const { id: windowId } = await browser.windows.create({});
      const tab2 = await browser.tabs.create({ windowId });
      browser.test.assertEq(1, tab2.index, `Two tabs in window ${windowId}`);

      const events = [];
      browser.tabGroups.onCreated.addListener(group => {
        browser.test.fail(`Unexpected tabGroups.onCreated: ${group.id}`);
      });
      browser.tabGroups.onRemoved.addListener(group => {
        browser.test.fail(`Unexpected tabGroups.onRemoved: ${group.id}`);
      });
      browser.tabGroups.onMoved.addListener(group => {
        browser.test.assertEq(groupId, group.id, "onMoved fired for group");
        events.push("tabGroups.onMoved");
      });
      browser.tabs.onMoved.addListener((movedTabId, moveInfo) => {
        // onDetached & onAttached should fire when moving to another window.
        browser.test.fail(
          `Unexpected tabs.onMoved: ${JSON.stringify(moveInfo)}`
        );
      });
      browser.tabs.onDetached.addListener((movedTabId, detachInfo) => {
        browser.test.assertEq(tabId, movedTabId, "Our tab detached");
        browser.test.assertEq(oldWinId, detachInfo.oldWindowId, "oldWindowId");
        browser.test.assertEq(1, detachInfo.oldPosition, "oldPosition");
        events.push("tabs.onDetached");
      });
      browser.tabs.onAttached.addListener((movedTabId, attachInfo) => {
        browser.test.assertEq(tabId, movedTabId, "Our tab attached");
        browser.test.assertEq(windowId, attachInfo.newWindowId, "newWindowId");
        browser.test.assertEq(1, attachInfo.newPosition, "newPosition");
        events.push("tabs.onAttached");
      });

      browser.test.assertDeepEq([], events, "No tabGroups event yet");
      // Move tab group between the (only) two existing tabs in the window.
      const moved = await browser.tabGroups.move(groupId, {
        windowId,
        index: 1,
      });
      browser.test.assertEq(groupId, moved.id, "Group ID did not change");
      browser.test.assertEq(windowId, moved.windowId, "Group moved to window");

      browser.test.assertEq(
        1,
        (await browser.tabs.get(tabId)).index,
        "Tab appears at the expected index"
      );

      await browser.windows.remove(windowId);
      browser.test.assertDeepEq(
        ["tabs.onDetached", "tabs.onAttached", "tabGroups.onMoved"],
        events,
        "Expected events when moving tab group to a new window"
      );

      browser.test.sendMessage("done");
    },
  });
  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();
});
