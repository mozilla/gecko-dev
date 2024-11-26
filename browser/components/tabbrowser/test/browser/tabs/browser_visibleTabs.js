/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function () {
  // There should be one tab when we start the test
  let [origTab] = gBrowser.visibleTabs;

  info("Add a tab that will get pinned");
  let pinned = BrowserTestUtils.addTab(gBrowser);
  gBrowser.pinTab(pinned);

  let testTab = BrowserTestUtils.addTab(gBrowser);

  let firefoxViewTab = BrowserTestUtils.addTab(gBrowser, "about:firefoxview");
  gBrowser.hideTab(firefoxViewTab);

  let visible = gBrowser.visibleTabs;
  is(visible.length, 3, "3 tabs should be visible");
  is(
    gBrowser.openTabs.length,
    4,
    "number of tabs to be considered open (step 1)"
  );
  is(visible[0], pinned, "the pinned tab is first");
  is(visible[1], origTab, "original tab is next");
  is(visible[2], testTab, "last created tab is next to last");

  info("Only show the test tab (but also get pinned and selected)");
  is(
    gBrowser.selectedTab,
    origTab,
    "sanity check that we're on the original tab"
  );
  BrowserTestUtils.showOnlyTheseTabs(gBrowser, [testTab]);
  is(gBrowser.visibleTabs.length, 3, "all 3 tabs are still visible");
  is(
    gBrowser.openTabs.length,
    4,
    "number of tabs to be considered open (step 2)"
  );

  info("Select the test tab and only show that (and pinned)");
  gBrowser.selectedTab = testTab;
  BrowserTestUtils.showOnlyTheseTabs(gBrowser, [testTab]);

  visible = gBrowser.visibleTabs;
  is(visible.length, 2, "2 tabs should be visible including the pinned");
  is(visible[0], pinned, "first is pinned");
  is(visible[1], testTab, "next is the test tab");
  is(gBrowser.tabs.length, 4, "4 tabs should still be open");
  is(
    gBrowser.openTabs.length,
    4,
    "number of tabs to be considered open (step 3)"
  );

  gBrowser.selectTabAtIndex(1);
  is(gBrowser.selectedTab, testTab, "second tab is the test tab");
  gBrowser.selectTabAtIndex(0);
  is(gBrowser.selectedTab, pinned, "first tab is pinned");
  gBrowser.selectTabAtIndex(2);
  is(gBrowser.selectedTab, testTab, "no third tab, so no change");
  gBrowser.selectTabAtIndex(0);
  is(gBrowser.selectedTab, pinned, "switch back to the pinned");
  gBrowser.selectTabAtIndex(2);
  is(gBrowser.selectedTab, testTab, "no third tab, so select last tab");
  gBrowser.selectTabAtIndex(-2);
  is(
    gBrowser.selectedTab,
    pinned,
    "pinned tab is second from left (when orig tab is hidden)"
  );
  gBrowser.selectTabAtIndex(-1);
  is(gBrowser.selectedTab, testTab, "last tab is the test tab");

  gBrowser.tabContainer.advanceSelectedTab(1, true);
  is(gBrowser.selectedTab, pinned, "wrapped around the end to pinned");
  gBrowser.tabContainer.advanceSelectedTab(1, true);
  is(gBrowser.selectedTab, testTab, "next to test tab");
  gBrowser.tabContainer.advanceSelectedTab(1, true);
  is(gBrowser.selectedTab, pinned, "next to pinned again");

  gBrowser.tabContainer.advanceSelectedTab(-1, true);
  is(gBrowser.selectedTab, testTab, "going backwards to last tab");
  gBrowser.tabContainer.advanceSelectedTab(-1, true);
  is(gBrowser.selectedTab, pinned, "next to pinned");
  gBrowser.tabContainer.advanceSelectedTab(-1, true);
  is(gBrowser.selectedTab, testTab, "next to test tab again");

  info("select a hidden tab thats selectable");
  gBrowser.selectedTab = firefoxViewTab;
  gBrowser.tabContainer.advanceSelectedTab(1, true);
  is(gBrowser.selectedTab, pinned, "next to first visible tab, the pinned tab");
  gBrowser.tabContainer.advanceSelectedTab(1, true);
  is(gBrowser.selectedTab, testTab, "next to second visible tab, the test tab");

  info("again select a hidden tab thats selectable");
  gBrowser.selectedTab = firefoxViewTab;
  gBrowser.tabContainer.advanceSelectedTab(-1, true);
  is(gBrowser.selectedTab, testTab, "next to last visible tab, the test tab");
  gBrowser.tabContainer.advanceSelectedTab(-1, true);
  is(gBrowser.selectedTab, pinned, "next to first visible tab, the pinned tab");

  info("Try showing all tabs except for the Firefox View tab");
  BrowserTestUtils.showOnlyTheseTabs(
    gBrowser,
    Array.from(gBrowser.tabs.slice(0, 3))
  );
  is(gBrowser.visibleTabs.length, 3, "all 3 tabs are visible again");
  is(
    gBrowser.openTabs.length,
    4,
    "number of tabs to be considered open (step 4)"
  );

  info(
    "Select the pinned tab and show the testTab to make sure selection updates"
  );
  gBrowser.selectedTab = pinned;
  BrowserTestUtils.showOnlyTheseTabs(gBrowser, [testTab]);
  is(gBrowser.tabs[1], origTab, "make sure origTab is in the middle");
  is(origTab.hidden, true, "make sure it's hidden");
  gBrowser.removeTab(pinned);
  is(gBrowser.selectedTab, testTab, "making sure origTab was skipped");
  is(gBrowser.visibleTabs.length, 1, "only testTab is there");
  is(
    gBrowser.openTabs.length,
    3,
    "number of tabs to be considered open (step 5)"
  );

  info("Only show one of the non-pinned tabs (but testTab is selected)");
  BrowserTestUtils.showOnlyTheseTabs(gBrowser, [origTab]);
  is(gBrowser.visibleTabs.length, 2, "got 2 tabs");
  is(
    gBrowser.openTabs.length,
    3,
    "number of tabs to be considered open (step 6)"
  );

  info("Now really only show one of the tabs");
  BrowserTestUtils.showOnlyTheseTabs(gBrowser, [testTab]);
  visible = gBrowser.visibleTabs;
  is(visible.length, 1, "only the original tab is visible");
  is(visible[0], testTab, "it's the original tab");
  is(gBrowser.tabs.length, 3, "still have 3 open tabs");
  is(
    gBrowser.openTabs.length,
    3,
    "number of tabs to be considered open (step 7)"
  );

  info("Close the selectable hidden tab");
  gBrowser.removeTab(firefoxViewTab);

  info(
    "Close the last visible tab and make sure we still get a visible tab with browser.tabs.closeWindowWithLastTab = false"
  );
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.closeWindowWithLastTab", false]],
  });
  gBrowser.removeTab(testTab);
  is(gBrowser.visibleTabs.length, 1, "a new visible tab was opened");
  is(gBrowser.tabs.length, 2, "we have two tabs in total");
  is(
    gBrowser.openTabs.length,
    2,
    "number of tabs to be considered open (step 8)"
  );
  ok(origTab.hidden, "original tab is still hidden");
  ok(!origTab.selected, "original tab is not selected");
  gBrowser.removeTab(origTab);
  await SpecialPowers.popPrefEnv();
});
