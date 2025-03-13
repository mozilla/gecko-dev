/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Ensure that a closed tab group will record all of its tabs into session state
// without being constrained by the `browser.sessionstore.max_tabs_undo` pref
// which normally limits the number of closed tabs that we retain in state.

const ORIG_STATE = SessionStore.getBrowserState();

const maxTabsUndo = Services.prefs.getIntPref(
  "browser.sessionstore.max_tabs_undo"
);
const numberOfTabsToTest = maxTabsUndo + 5;

registerCleanupFunction(async () => {
  forgetClosedWindows();
  forgetSavedTabGroups();
  await SessionStoreTestUtils.promiseBrowserState(ORIG_STATE);
});

add_task(async function test_restoreClosedTabGroupWithManyTabs() {
  let win = await promiseNewWindowLoaded();
  let tabs = [];
  for (let i = 1; i <= numberOfTabsToTest; i++) {
    let tab = BrowserTestUtils.addTab(
      win.gBrowser,
      `https://example.com/?${i}`
    );
    tabs.push(tab);
  }
  await Promise.all(
    tabs.map(tab => BrowserTestUtils.browserLoaded(tab.linkedBrowser))
  );
  await Promise.all(tabs.map(tab => TabStateFlusher.flush(tab.linkedBrowser)));

  const tabGroupToClose = win.gBrowser.addTabGroup(tabs, {
    color: "blue",
    label: "many tabs",
  });
  const tabGroupToCloseId = tabGroupToClose.id;

  await TabStateFlusher.flushWindow(win);

  Assert.equal(
    win.gBrowser.tabs.length,
    numberOfTabsToTest + 1,
    `there should be ${numberOfTabsToTest} new tabs + 1 initial tab from the new window`
  );

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

  await TabStateFlusher.flushWindow(win);

  let closedTabGroups = ss.getClosedTabGroups(win);
  Assert.equal(
    closedTabGroups.length,
    1,
    "one closed tab group should be in session state"
  );
  let closedTabGroup = closedTabGroups[0];
  Assert.equal(
    closedTabGroup.id,
    tabGroupToCloseId,
    "the tab group we closed should be in session state"
  );
  Assert.equal(
    closedTabGroup.tabs.length,
    numberOfTabsToTest,
    "the closed tab group should contain all of the tabs"
  );

  let restorePromise = BrowserTestUtils.waitForEvent(win, "SSWindowStateReady");
  SessionStore.undoCloseTabGroup(win, tabGroupToCloseId, win);
  await restorePromise;

  Assert.equal(
    win.gBrowser.tabs.length,
    numberOfTabsToTest + 1,
    `there should be ${numberOfTabsToTest} tabs restored + 1 initial tab from the new window`
  );

  await BrowserTestUtils.closeWindow(win);
});
