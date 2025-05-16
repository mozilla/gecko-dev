/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// TODO bug 1938594: moving a tab to another window sometimes triggers this
// error. See https://bugzilla.mozilla.org/show_bug.cgi?id=1966823#c3 for an
// analysis, copied to: https://bugzilla.mozilla.org/show_bug.cgi?id=1938594#c4
PromiseTestUtils.allowMatchingRejectionsGlobally(
  /Unexpected undefined tabState for onMoveToNewWindow/
);

add_task(async function test_tabGroups_move_index() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabGroups"],
    },
    async background() {
      const firstTab = await browser.tabs.create({ url: "about:blank#0" });

      // In this test we are going to reuse many tabs; to make tests easier to
      // understand, the tests are going to reference tabs by their initial
      // index in the tab. This array maps the tabs at the "initial index" to
      // the actual tab ID.
      let reusableTabIds = [firstTab.id];

      // We are also going to reuse the window. When there are more tabs than
      // we need, we'll put the excess tabs in the initial window.
      const nonTestWindowId = firstTab.windowId;
      const testWindow = await browser.windows.create({ tabId: firstTab.id });
      const windowId = testWindow.id; // Used in the whole test!
      let numberOfTabsInTestWindow = 1;

      async function setTabCountInTestWindow(tabCount) {
        await browser.tabs.ungroup(
          reusableTabIds.slice(0, numberOfTabsInTestWindow)
        );
        for (let tabId of reusableTabIds) {
          await browser.tabs.update(tabId, { pinned: false });
        }
        for (let i = 0; i < tabCount; ++i) {
          if (i in reusableTabIds) {
            await browser.tabs.move(reusableTabIds[i], { windowId, index: i });
          } else {
            const tab = await browser.tabs.create({
              url: `about:blank#${i}`,
              windowId,
              index: i,
            });
            reusableTabIds[i] = tab.id;
          }
        }
        // Move excess tabs to another window.
        for (let i = tabCount; i < numberOfTabsInTestWindow; ++i) {
          await browser.tabs.move(reusableTabIds[i], {
            windowId: nonTestWindowId,
            index: -1,
          });
        }
        numberOfTabsInTestWindow = tabCount;
      }

      // Test the behavior of tabGroups.move, for a given initial tab strip,
      // in the format [0, 1, 2, [3, 4, 5], 6, etc], where the numbers reflect
      // the tabs (by their initial index), and the nested array marks a group.
      async function testTabGroupMove(testCase) {
        browser.test.log(`testTabGroupMove: ${JSON.stringify(testCase)}`);
        const DUMMY_TABGROUPS_MOVE_NO_ERROR = "(tabGroups.move succeeded)";
        const {
          description,
          starting_tabstrip,
          pinned_count = 0,
          group_at_tab,
          index,
          expected_tabstrip = starting_tabstrip,
          expected_error = DUMMY_TABGROUPS_MOVE_NO_ERROR,
        } = testCase;
        let allTabIndexes = starting_tabstrip.flatMap(num => num);
        // Sanity check: each number reflects the initial index.
        if (allTabIndexes.some((num, i) => num !== i)) {
          browser.test.fail(`Bad starting_tabstrip: ${starting_tabstrip}`);
        }

        // Setup: Prepare window as specified by starting_tabstrip, with a
        // group for each array.
        await setTabCountInTestWindow(allTabIndexes.length);
        const groupIdAtIndex = new Map();
        for (let v of starting_tabstrip) {
          if (Array.isArray(v)) {
            const groupId = await browser.tabs.group({
              createProperties: { windowId },
              tabIds: v.map(i => reusableTabIds[i]),
            });
            groupIdAtIndex.set(v[0], groupId);
          }
        }
        for (let i = 0; i < pinned_count; ++i) {
          let tabId = reusableTabIds[i];
          await browser.tabs.update(tabId, { pinned: true });
        }

        if (!groupIdAtIndex.has(group_at_tab)) {
          // Sanity check: group starts at the given index.
          browser.test.fail("Bad group_at_tab: no group found at index");
        }

        const groupId = groupIdAtIndex.get(group_at_tab);
        let actualError;
        try {
          await browser.tabGroups.move(groupId, { index });
          actualError = DUMMY_TABGROUPS_MOVE_NO_ERROR;
        } catch (e) {
          actualError = e.message;
        }
        browser.test.assertEq(
          expected_error,
          actualError,
          `expected_error matches - ${description}`
        );

        const actualTabstrip = [];
        let lastGroupId;
        for (const tab of await browser.tabs.query({ windowId })) {
          let initialTabStripIndex = reusableTabIds.indexOf(tab.id);
          if (tab.groupId == -1) {
            actualTabstrip.push(initialTabStripIndex);
          } else if (lastGroupId == tab.groupId) {
            actualTabstrip.at(-1).push(initialTabStripIndex);
          } else {
            actualTabstrip.push([initialTabStripIndex]);
          }
          lastGroupId = tab.groupId;
        }
        browser.test.assertDeepEq(
          expected_tabstrip,
          actualTabstrip,
          `Got expected tabstrip after move - ${description}`
        );
      }

      browser.test.log("Initial test window created - running tests now.");
      const ERROR_MOVE_IN_GROUP =
        "Cannot move the group to an index that is in the middle of another group.";
      const ERROR_MOVE_TO_PINNED =
        "Cannot move the group to an index that is in the middle of pinned tabs.";

      await testTabGroupMove({
        description: "Move group that is alone in the window to same position",
        starting_tabstrip: [[0]],
        group_at_tab: 0,
        index: 0,
        expected_tabstrip: [[0]],
      });

      await testTabGroupMove({
        description: "Move group that is alone in the window to last position",
        starting_tabstrip: [[0]],
        group_at_tab: 0,
        index: -1,
        expected_tabstrip: [[0]],
      });

      await testTabGroupMove({
        description: "Move group to first position",
        starting_tabstrip: [0, [1, 2, 3], 4],
        group_at_tab: 1,
        index: 0,
        expected_tabstrip: [[1, 2, 3], 0, 4],
      });

      await testTabGroupMove({
        description: "Move group to last position",
        starting_tabstrip: [0, [1, 2, 3], 4],
        group_at_tab: 1,
        index: -1,
        expected_tabstrip: [0, 4, [1, 2, 3]],
      });

      await testTabGroupMove({
        description: "Move group to first position inside original group",
        starting_tabstrip: [0, 1, [2, 3, 4, 5], 6, 7, 8, 9],
        group_at_tab: 2,
        index: 3,
        expected_tabstrip: [0, 1, 6, [2, 3, 4, 5], 7, 8, 9],
      });

      await testTabGroupMove({
        description: "Move group to last position inside original group",
        starting_tabstrip: [0, 1, [2, 3, 4, 5], 6, 7, 8, 9],
        group_at_tab: 2,
        index: 5,
        expected_tabstrip: [0, 1, 6, 7, 8, [2, 3, 4, 5], 9],
      });

      await testTabGroupMove({
        description: "Move group to position of right neighbor",
        starting_tabstrip: [0, 1, [2, 3, 4, 5], 6, 7, 8, 9],
        group_at_tab: 2,
        index: 6,
        expected_tabstrip: [0, 1, 6, 7, 8, 9, [2, 3, 4, 5]],
      });

      await testTabGroupMove({
        description: "Move group to position past right neighbor (clamped)",
        starting_tabstrip: [0, 1, [2, 3, 4, 5], 6, 7, 8, 9],
        group_at_tab: 2,
        index: 7,
        expected_tabstrip: [0, 1, 6, 7, 8, 9, [2, 3, 4, 5]],
        //                                        ^
        //                        This is actually index 7.
        //                        But we cannot leave a gap before the group,
        //                        so the group ends up at index 6 instead.
      });

      await testTabGroupMove({
        description: "Move group to position past end of tab strip (clamped)",
        starting_tabstrip: [0, 1, [2, 3, 4, 5], 6, 7, 8, 9],
        group_at_tab: 2,
        index: 10,
        expected_tabstrip: [0, 1, 6, 7, 8, 9, [2, 3, 4, 5]],
      });

      // Splits ////////////////////////////////////////////////////////////////
      // Moving a group between other groups

      await testTabGroupMove({
        description: "Move group (size 1) left between 1-length groups",
        starting_tabstrip: [0, [1], [2], [3, 4, 5], 6, 7, [8], 9],
        group_at_tab: 8,
        index: 2,
        expected_tabstrip: [0, [1], [8], [2], [3, 4, 5], 6, 7, 9],
      });

      await testTabGroupMove({
        description: "Move group (size 1) right between 1-length groups",
        starting_tabstrip: [0, [1], [2], [3, 4, 5], [6], [7], 8, [9]],
        group_at_tab: 1,
        index: 6,
        expected_tabstrip: [0, [2], [3, 4, 5], [6], [1], [7], 8, [9]],
      });

      await testTabGroupMove({
        description: "Move group (size 1) left before a length-1 group",
        starting_tabstrip: [0, [1], [2], [3, 4, 5], 6, 7, [8], 9],
        group_at_tab: 8,
        index: 1,
        expected_tabstrip: [0, [8], [1], [2], [3, 4, 5], 6, 7, 9],
      });

      await testTabGroupMove({
        description: "Move group (size 4) left between other groups",
        starting_tabstrip: [[0], [1, 2], [3, 4, 5], [6, 7, 8, 9]],
        group_at_tab: 6,
        index: 1,
        expected_tabstrip: [[0], [6, 7, 8, 9], [1, 2], [3, 4, 5]],
      });

      await testTabGroupMove({
        description: "Move group (size 2) right between other groups",
        starting_tabstrip: [[0], [1, 2], [3, 4, 5], [6, 7, 8, 9]],
        group_at_tab: 1,
        index: 4,
        expected_tabstrip: [[0], [3, 4, 5], [1, 2], [6, 7, 8, 9]],
      });

      // Intersections /////////////////////////////////////////////////////////
      // Moving tab groups into other tab groups

      await testTabGroupMove({
        description: "Move group (size 1) right inside another group (size 3)",
        starting_tabstrip: [0, 1, [2], [3, 4, 5], 6, 7, 8, 9],
        group_at_tab: 2,
        index: 3,
        expected_error: ERROR_MOVE_IN_GROUP,
      });

      await testTabGroupMove({
        description: "Move group (size 1) left inside another group (size 3)",
        starting_tabstrip: [0, 1, [2, 3, 4], 5, 6, [7], 8, 9],
        group_at_tab: 7,
        index: 3,
        expected_error: ERROR_MOVE_IN_GROUP,
      });

      await testTabGroupMove({
        description: "Move group (size 2) right inside another group (size 5)",
        starting_tabstrip: [0, [1, 2], 3, [4, 5, 6, 7, 8], 9],
        group_at_tab: 1,
        index: 6,
        expected_error: ERROR_MOVE_IN_GROUP,
      });

      await testTabGroupMove({
        description: "Move group (size 5) left inside another group (size 3)",
        starting_tabstrip: [[0, 1, 2], 3, [4, 5, 6, 7, 8], 9],
        group_at_tab: 4,
        index: 1,
        expected_error: ERROR_MOVE_IN_GROUP,
      });

      // Pinned tabs ///////////////////////////////////////////////////////////

      await testTabGroupMove({
        description: "Move group before pinned tab",
        starting_tabstrip: [0, 1, [2], 3],
        pinned_count: 2,
        group_at_tab: 2,
        index: 0,
        expected_error: ERROR_MOVE_TO_PINNED,
      });

      await testTabGroupMove({
        description: "Move group between pinned tabs",
        starting_tabstrip: [0, 1, [2], 3],
        pinned_count: 2,
        group_at_tab: 2,
        index: 1,
        expected_error: ERROR_MOVE_TO_PINNED,
      });

      await testTabGroupMove({
        description: "Move group after pinned tab (position not changed)",
        starting_tabstrip: [0, 1, [2], 3],
        pinned_count: 2,
        group_at_tab: 2,
        index: 2,
        expected_tabstrip: [0, 1, [2], 3],
      });

      await testTabGroupMove({
        description: "Move group after pinned tab",
        starting_tabstrip: [0, 1, 2, [3]],
        pinned_count: 2,
        group_at_tab: 3,
        index: 2,
        expected_tabstrip: [0, 1, [3], 2],
      });

      browser.test.log("Tests done - cleaning up.");
      for (let tabId of reusableTabIds) {
        // This also closes testWindow.
        await browser.tabs.remove(tabId);
      }

      browser.test.sendMessage("done");
    },
  });
  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();
});
