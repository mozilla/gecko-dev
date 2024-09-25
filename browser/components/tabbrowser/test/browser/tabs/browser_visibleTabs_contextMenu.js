/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test() {
  // There should be one tab when we start the test
  let [origTab] = gBrowser.visibleTabs;
  is(gBrowser.visibleTabs.length, 1, "there is one visible tab");
  let testTab = BrowserTestUtils.addTab(gBrowser);
  is(gBrowser.visibleTabs.length, 2, "there are now two visible tabs");

  // Check the context menu with two tabs
  updateTabContextMenu(origTab);
  ok(
    !document.getElementById("context_closeTab").disabled,
    "Close Tab is enabled"
  );

  // Hide the original tab.
  gBrowser.selectedTab = testTab;
  gBrowser.showOnlyTheseTabs([testTab]);
  is(gBrowser.visibleTabs.length, 1, "now there is only one visible tab");

  // Check the context menu with one tab.
  updateTabContextMenu(testTab);
  ok(
    !document.getElementById("context_closeTab").disabled,
    "Close Tab is enabled when more than one tab exists"
  );
  ok(
    !document.getElementById("context_closeDuplicateTabs").disabled,
    "Close duplicate tabs is enabled when more than one tab with the same URL exists"
  );

  // Add a tab that will get pinned
  // So now there's one pinned tab, one visible unpinned tab, and one hidden tab
  let pinned = BrowserTestUtils.addTab(gBrowser);
  gBrowser.pinTab(pinned);
  is(gBrowser.visibleTabs.length, 2, "now there are two visible tabs");

  // Check the context menu on the pinned tab
  updateTabContextMenu(pinned);
  ok(
    !document.getElementById("context_closeTabOptions").disabled,
    "Close Multiple Tabs is enabled on pinned tab"
  );
  ok(
    !document.getElementById("context_closeOtherTabs").disabled,
    "Close Other Tabs is enabled on pinned tab"
  );
  ok(
    document.getElementById("context_closeTabsToTheStart").disabled,
    "Close Tabs To The Start is disabled on pinned tab"
  );
  ok(
    !document.getElementById("context_closeTabsToTheEnd").disabled,
    "Close Tabs To The End is enabled on pinned tab"
  );

  // Check the context menu on the unpinned visible tab
  updateTabContextMenu(testTab);
  ok(
    document.getElementById("context_closeTabOptions").disabled,
    "Close Multiple Tabs is disabled on single unpinned tab"
  );
  ok(
    document.getElementById("context_closeOtherTabs").disabled,
    "Close Other Tabs is disabled on single unpinned tab"
  );
  ok(
    document.getElementById("context_closeTabsToTheStart").disabled,
    "Close Tabs To The Start is disabled on single unpinned tab"
  );
  ok(
    document.getElementById("context_closeTabsToTheEnd").disabled,
    "Close Tabs To The End is disabled on single unpinned tab"
  );

  // Show all tabs
  let allTabs = Array.from(gBrowser.tabs);
  gBrowser.showOnlyTheseTabs(allTabs);

  // Check the context menu now
  updateTabContextMenu(testTab);
  ok(
    !document.getElementById("context_closeTabOptions").disabled,
    "Close Multiple Tabs is enabled on unpinned tab when there's another unpinned tab"
  );
  ok(
    !document.getElementById("context_closeOtherTabs").disabled,
    "Close Other Tabs is enabled on unpinned tab when there's another unpinned tab"
  );
  ok(
    !document.getElementById("context_closeTabsToTheStart").disabled,
    "Close Tabs To The Start is enabled on last unpinned tab when there's another unpinned tab"
  );
  ok(
    document.getElementById("context_closeTabsToTheEnd").disabled,
    "Close Tabs To The End is disabled on last unpinned tab"
  );

  // Check the context menu of the original tab
  // Close Tabs To The End should now be enabled
  updateTabContextMenu(origTab);
  ok(
    !document.getElementById("context_closeTabsToTheEnd").disabled,
    "Close Tabs To The End is enabled on unpinned tab when followed by another"
  );

  // flip the pref to move the tabstrip into the sidebar
  await SpecialPowers.pushPrefEnv({ set: [["sidebar.verticalTabs", true]] });

  // Check the context menu now
  updateTabContextMenu(testTab);
  is(
    document.getElementById("context_openANewTab").dataset.l10nId,
    "tab-context-new-tab-open-vertical",
    "Has correct new tab string for vertical tabs"
  );
  is(
    document.getElementById("context_closeTabsToTheEnd").dataset.l10nId,
    "close-tabs-to-the-end-vertical",
    "Close multiple tabs has correct close tabs at end string for vertial tabs"
  );

  is(
    document.getElementById("context_closeTabsToTheStart").dataset.l10nId,
    "close-tabs-to-the-start-vertical",
    "Close multiple tabs has correct close tabs at start string for vertial tabs"
  );

  // flip the pref to move the tabstrip back horizontally
  await SpecialPowers.pushPrefEnv({ set: [["sidebar.verticalTabs", false]] });

  // Check context menu item strings has been reset horizontal tabs
  updateTabContextMenu(testTab);

  is(
    document.getElementById("context_openANewTab").dataset.l10nId,
    "tab-context-new-tab-open",
    "Has correct new tab string for horizontal tabs"
  );
  is(
    document.getElementById("context_closeTabsToTheStart").dataset.l10nId,
    "close-tabs-to-the-start",
    "Close multiple tabs has correct close tabs at start string for horizontal tabs"
  );
  is(
    document.getElementById("context_closeTabsToTheEnd").dataset.l10nId,
    "close-tabs-to-the-end",
    "Close multiple tabs has correct close tabs at end string for horizontal tabs"
  );

  gBrowser.removeTab(testTab);
  gBrowser.removeTab(pinned);
});
