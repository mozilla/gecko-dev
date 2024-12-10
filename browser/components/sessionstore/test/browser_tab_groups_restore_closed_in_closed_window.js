/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const ORIG_STATE = SessionStore.getBrowserState();
forgetClosedWindows();

registerCleanupFunction(async () => {
  await SessionStoreTestUtils.promiseBrowserState(ORIG_STATE);
});

add_task(async function test_restoreClosedTabGroupFromClosedWindow() {
  let win = await promiseNewWindowLoaded();
  let aboutMozillaTab = BrowserTestUtils.addTab(win.gBrowser, "about:mozilla");
  let aboutRobotsTab = BrowserTestUtils.addTab(win.gBrowser, "about:robots");
  let aboutCrashesTab = BrowserTestUtils.addTab(win.gBrowser, "about:crashes");
  const tabGroupToClose = win.gBrowser.addTabGroup(
    [aboutRobotsTab, aboutCrashesTab],
    {
      color: "blue",
      label: "about pages",
    }
  );
  const tabGroupToCloseId = tabGroupToClose.id;
  await Promise.all([
    BrowserTestUtils.browserLoaded(
      aboutMozillaTab.linkedBrowser,
      false,
      "about:mozilla"
    ),
    BrowserTestUtils.browserLoaded(
      aboutRobotsTab.linkedBrowser,
      false,
      "about:robots"
    ),
    BrowserTestUtils.browserLoaded(
      aboutCrashesTab.linkedBrowser,
      false,
      "about:crashes"
    ),
  ]);

  await TabStateFlusher.flushWindow(win);
  await forceSaveState();

  let removePromise = BrowserTestUtils.waitForEvent(
    tabGroupToClose,
    "TabGroupRemoved"
  );
  win.gBrowser.removeTabGroup(tabGroupToClose);
  await removePromise;

  Assert.ok(
    !win.gBrowser.tabGroups.length,
    "closed tab group should not be in the tab strip"
  );
  Assert.ok(
    !win.gBrowser.tabs.includes(aboutRobotsTab),
    "about:robots tab should not be in the tab strip"
  );
  Assert.ok(
    !win.gBrowser.tabs.includes(aboutCrashesTab),
    "about:crashes tab should not be in the tab strip"
  );

  await TabStateFlusher.flushWindow(win);
  await forceSaveState();

  const closedTabGroups = SessionStore.getClosedTabGroups(win);
  Assert.equal(closedTabGroups.length, 1, "there should be a closed tab group");
  Assert.equal(
    closedTabGroups[0].id,
    tabGroupToCloseId,
    "the closed tab group should be the one we just closed"
  );

  let win2 = await promiseNewWindowLoaded();
  await BrowserTestUtils.closeWindow(win);

  const closedWindowData = SessionStore.getClosedWindowData();
  Assert.equal(closedWindowData.length, 1, "only `win` should be closed");

  let restorePromise = BrowserTestUtils.waitForEvent(
    win2,
    "SSWindowStateReady"
  );
  const restoredTabGroup = SessionStore.undoCloseTabGroup(
    { sourceClosedId: closedWindowData[0].closedId },
    tabGroupToCloseId,
    win2
  );
  await restorePromise;

  Assert.equal(
    win2.gBrowser.tabs.length,
    3,
    "there should be 2 tabs restored + 1 initial tab from the new window"
  );
  Assert.equal(
    win2.gBrowser.tabGroups.length,
    1,
    "there should be 1 tab group restored"
  );

  Assert.equal(
    restoredTabGroup.tabs.length,
    2,
    "the 2 restored tabs should be in the restored tab group"
  );
  Assert.equal(
    restoredTabGroup.label,
    "about pages",
    "tab group name should be restored"
  );
  Assert.equal(
    restoredTabGroup.id,
    tabGroupToCloseId,
    "tab group ID should be restored"
  );
  Assert.equal(
    restoredTabGroup.color,
    "blue",
    "tab group color should be restored"
  );
  Assert.ok(
    !restoredTabGroup.collapsed,
    "tab group collapsed state should be restored"
  );

  await BrowserTestUtils.closeWindow(win2);
  forgetClosedWindows();
});
