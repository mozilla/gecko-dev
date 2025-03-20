/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function () {
  Assert.equal(gBrowser.tabs.length, 1, "Starting with one tab");

  let firstTab = gBrowser.tabs[0];
  Assert.ok(!firstTab.pinned, "First tab isn't pinned initially");

  let unpinnedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  if (gReduceMotion || gBrowser.tabContainer.verticalMode) {
    info(
      "Tab animations are disabled, so not waiting for the tab open animation"
    );
  } else {
    info("waiting for the tab open animation to finish");
    await BrowserTestUtils.waitForEvent(unpinnedTab, "TabAnimationEnd");
  }

  gBrowser.pinTab(firstTab);
  Assert.equal(gBrowser.tabs.length, 2, "We have two tabs now");
  Assert.ok(firstTab.pinned, "Successfully pinned the first tab");

  info("clicking the close button via mouse to enable tab size locking");
  EventUtils.synthesizeMouseAtCenter(unpinnedTab.closeButton, {}, window);
  if (gReduceMotion || gBrowser.tabContainer.verticalMode) {
    info(
      "Tab animations are disabled, so not waiting for the tab close animation"
    );
  } else {
    info(
      "waiting for tab animation to finish and the tab to close after clicking its close button"
    );
    await BrowserTestUtils.waitForEvent(unpinnedTab, "TabAnimationEnd");
  }

  Assert.equal(
    gBrowser.tabs.length,
    1,
    "Successfully removed the unpinned tab"
  );
  Assert.ok(firstTab.pinned, "First tab is now a standalone pinned tab");

  gBrowser.unpinTab(firstTab);
  Assert.ok(!firstTab.pinned, "Successfully unpinned the first tab");
});
