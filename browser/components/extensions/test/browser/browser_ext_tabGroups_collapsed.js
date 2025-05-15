/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Regression test for https://bugzilla.mozilla.org/show_bug.cgi?id=1966617
// Tests the following scenarios:
// - tabs.move into collapsed group.
// - tabGroups.move to position of hidden tab.
// - tabGroups.move before/inside/after collapsed group.
add_task(async function test_adopt_tab_or_group_around_collapsed_group() {
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    // ^ tabHide triggers notification via ExtensionControlledPopup, which
    // expects an addon (for metadata). So we need to use useAddonManager.
    manifest: {
      permissions: ["tabGroups", "tabHide"],
    },
    async background() {
      // This is the tab that we're going to move.
      const { id: tabId, windowId: otherWinId } = await browser.tabs.create({});

      // Window to move to, consisting of 6 tabs:
      // - index 0: hidden tab
      // - index 1, 2, 3: tabs in collapsed tab group
      // - index 4: normal tab
      // - index 5: hidden tab
      const {
        id: windowId,
        tabs: [{ id: tabId0 }],
      } = await browser.windows.create({});
      const { id: tabId1 } = await browser.tabs.create({ windowId });
      const { id: tabId2 } = await browser.tabs.create({ windowId });
      const { id: tabId3 } = await browser.tabs.create({ windowId });
      const { id: tabId4 } = await browser.tabs.create({ windowId });
      const { id: tabId5 } = await browser.tabs.create({ windowId });
      const groupId = await browser.tabs.group({
        createProperties: { windowId },
        tabIds: [tabId1, tabId2, tabId3],
      });
      await browser.tabs.hide(tabId0);
      await browser.tabGroups.update(groupId, { collapsed: true });
      await browser.tabs.update(tabId4, { active: true });
      await browser.tabs.hide(tabId5);

      const tabs = await browser.tabs.query({ windowId });

      // "hidden" property mirrors state from tabs.hide(), independent of
      // collapsed group state.
      browser.test.assertDeepEq(
        [true, false, false, false, false, true],
        tabs.map(t => t.hidden),
        "tabs.Tab.hidden property is true for hidden tabs only (not collapsed)"
      );

      browser.test.assertDeepEq(
        [-1, groupId, groupId, groupId, -1, -1],
        tabs.map(t => t.groupId),
        "Sanity check: Three of the tabs are in the expected tab group"
      );

      // At the end of each tab-moving test, the test tab (tabId) is located
      // in the test window (windowId). To make sure that the next test
      // exercises the "adopt tab in new window" code path, force the tab to
      // return to the other window.
      async function putTabInGroupInOtherWindow() {
        const newGroupId = await browser.tabs.group({
          tabIds: tabId,
          createProperties: { windowId: otherWinId },
        });
        const tab = await browser.tabs.get(tabId);
        browser.test.assertDeepEq(
          { groupId: newGroupId, windowId: otherWinId },
          { groupId: tab.groupId, windowId: tab.windowId },
          "Sanity check: tab is in group back in other window"
        );
        return newGroupId;
      }

      // Set up done and sanity checks completed, now perform the move test!
      let [movedTab] = await browser.tabs.move(tabId, { windowId, index: 2 });
      browser.test.assertDeepEq(
        { index: 2, groupId },
        { index: movedTab.index, groupId: movedTab.groupId },
        "Tab moved to position between invisible tabs in collapsed group"
      );

      // Test moving tab group before collapsed tab group, after the hidden tab.
      const groupIdToMove1 = await putTabInGroupInOtherWindow();
      await browser.tabGroups.move(groupIdToMove1, { windowId, index: 1 });
      const movedTab1 = await browser.tabs.get(tabId);
      browser.test.assertDeepEq(
        { index: 1, groupId: groupIdToMove1, windowId },
        {
          index: movedTab1.index,
          groupId: movedTab1.groupId,
          windowId: movedTab1.windowId,
        },
        "Tab group moved to position before collapsed group, after hidden tab"
      );

      const groupIdToMove2 = await putTabInGroupInOtherWindow();
      await browser.test.assertRejects(
        browser.tabGroups.move(groupIdToMove2, { windowId, index: 2 }),
        "Cannot move the group to an index that is in the middle of another group.",
        "Should reject attempt to move group in middle of group"
      );

      const groupIdToMove3 = await putTabInGroupInOtherWindow();
      await browser.test.assertRejects(
        browser.tabGroups.move(groupIdToMove3, { windowId, index: 3 }),
        "Cannot move the group to an index that is in the middle of another group.",
        "Should reject attempt to move before end of group"
      );

      const groupIdToMove4 = await putTabInGroupInOtherWindow();
      await browser.tabGroups.move(groupIdToMove4, { windowId, index: 4 });
      const movedTab4 = await browser.tabs.get(tabId);
      browser.test.assertDeepEq(
        { index: 4, groupId: groupIdToMove4, windowId },
        {
          index: movedTab4.index,
          groupId: movedTab4.groupId,
          windowId: movedTab4.windowId,
        },
        "Tab group moved to position after collapsed group"
      );

      const groupIdToMove5 = await putTabInGroupInOtherWindow();
      await browser.tabGroups.move(groupIdToMove5, { windowId, index: 5 });
      const movedTab5 = await browser.tabs.get(tabId);
      browser.test.assertDeepEq(
        { index: 5, groupId: groupIdToMove5, windowId },
        {
          index: movedTab5.index,
          groupId: movedTab5.groupId,
          windowId: movedTab5.windowId,
        },
        "Tab group moved to position before the last tab (which is hidden)"
      );

      // Test moving tab group to end. This should NOT ignore hidden tabs.
      // Test cases:
      //  6 is the true position of the last tab.
      // -1 is the documented alias for "last position"
      //  7 is past the last position, but clamps to the end.
      // 1000 is way past the last position, and still clamps to the end.
      for (let index of [6, -1, 7, 1000]) {
        const groupIdToMoveEnd = await putTabInGroupInOtherWindow();
        await browser.tabGroups.move(groupIdToMoveEnd, { windowId, index });
        const movedTabEnd = await browser.tabs.get(tabId);
        browser.test.assertDeepEq(
          { index: 6, groupId: groupIdToMoveEnd, windowId },
          {
            index: movedTabEnd.index,
            groupId: movedTabEnd.groupId,
            windowId: movedTabEnd.windowId,
          },
          `Tab group moved to position at end (${index}), after hidden tab`
        );
      }

      await browser.windows.remove(windowId);

      browser.test.sendMessage("done");
    },
  });
  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();
});
