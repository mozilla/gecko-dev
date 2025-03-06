/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(() =>
  SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.groups.enabled", true]],
  })
);

// Validates that tab widths get locked while a user is in the midst of closing
// tabs. However, also validates that all tabs have their widths unlocked afterward
// so that tabs can visually shrink/grow.
add_task(async function test_lockTabSizing_resets_collapsed_tab_groups() {
  const [tab1, tab2, tab3] = await Promise.all([
    addTab("about:blank"),
    addTab("about:blank"),
    addTab("about:blank"),
  ]);
  const tabGroup = gBrowser.addTabGroup([tab1], { insertBefore: tab1 });

  info(
    "close a tab that isn't the last tab by mouse in order to trigger tab width locking"
  );
  await triggerMiddleClickOn(tab2);
  Assert.ok(
    gBrowser.visibleTabs.every(tab => tab.style.maxWidth),
    "tab widths of visible tabs should be locked"
  );

  info("collapse the tab group, making its tabs no longer visible");
  tabGroup.collapsed = true;

  info(
    "close the last tab without the mouse to end the period of locked tab widths"
  );
  BrowserTestUtils.removeTab(tab3);

  Assert.ok(
    gBrowser.tabs.every(tab => !tab.style.maxWidth),
    "tab widths of all tabs should be unlocked"
  );

  // Clean up the tab group + tabs we created.
  gBrowser.removeTabGroup(tabGroup);
});
