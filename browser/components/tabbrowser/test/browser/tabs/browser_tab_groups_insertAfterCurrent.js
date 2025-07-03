/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.groups.enabled", true],
      ["browser.tabs.insertAfterCurrent", true],
      ["browser.tabs.closeWindowWithLastTab", false],
    ],
  });
});

/**
 * Tests that closing the last group when that is all there is on the tab bar
 * causes a new tab to be created outside of the group.
 */
add_task(async function test_closeSolitaryGroup() {
  let currentTab = gBrowser.selectedTab;
  let additionalTab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    skipAnimation: true,
  });
  let group = gBrowser.addTabGroup([currentTab, additionalTab]);

  await removeTabGroup(group);

  Assert.equal(
    gBrowser.tabGroups.length,
    0,
    "there are no groups in the tabstrip"
  );
  Assert.equal(gBrowser.tabs.length, 1, "there is one tab in the tabstrip");
});
