/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Validates that all ungrouped/standalone tabs have an explicit `aria-level="1"`
// while all tabs inside of tab groups have an explicit `aria-level="2"`. These
// attributes help users to understand the hierarchy of the tab strip, which we
// mainly communicate to users via visual outlines on the tab strip.

function forgetSavedTabGroups() {
  const tabGroups = SessionStore.getSavedTabGroups();
  tabGroups.forEach(tabGroup => SessionStore.forgetSavedTabGroup(tabGroup.id));
}

add_task(async function test_ARIA_level_on_tabs() {
  const tabs = await Promise.all(
    Array.from({ length: 8 }).map((_, index) =>
      addTab(`http://mochi.test:8888/${index}`)
    )
  );
  const [_0, tab1, tab2, _3, _4, _5, tab6, tab7] = tabs;

  const group1 = gBrowser.addTabGroup([tab1, tab2], {
    insertBefore: tab1,
  });
  await BrowserTestUtils.waitForEvent(group1, "TabGrouped");

  const group2 = gBrowser.addTabGroup([tab6, tab7], {
    insertBefore: tab6,
  });
  await BrowserTestUtils.waitForEvent(group2, "TabGrouped");

  Assert.ok(
    gBrowser.tabs
      .filter(tab => !tab.group)
      .every(tab => tab.getAttribute("aria-level") == "1"),
    "all tabs outside of tab groups have aria-level='1'"
  );

  Assert.ok(
    gBrowser.tabs
      .filter(tab => tab.group)
      .every(tab => tab.getAttribute("aria-level") == "2"),
    "all tabs inside of tab groups have aria-level='2'"
  );

  for (const tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }
  forgetSavedTabGroups();
});
