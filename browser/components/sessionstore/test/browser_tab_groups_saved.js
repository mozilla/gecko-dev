"use strict";

const ORIG_STATE = SessionStore.getBrowserState();

registerCleanupFunction(async () => {
  await SessionStoreTestUtils.promiseBrowserState(ORIG_STATE);
});

add_task(async function test_SaveTabGroups() {
  let win = await promiseNewWindowLoaded();
  let state = ss.getCurrentState(win);
  Assert.equal(
    state.savedGroups.length,
    0,
    "savedGroups starts in initial state"
  );

  let tab1 = BrowserTestUtils.addTab(win.gBrowser, "about:blank");
  let group1 = win.gBrowser.addTabGroup([tab1]);
  ss.addSavedTabGroup(group1);

  let tab2 = BrowserTestUtils.addTab(win.gBrowser, "about:blank");
  let group2 = win.gBrowser.addTabGroup([tab2]);
  ss.addSavedTabGroup(group2);

  state = ss.getCurrentState();

  Assert.equal(state.savedGroups.length, 2, "savedGroups has 2 groups");
  Assert.equal(state.savedGroups[0].id, group1.id, "group1 is in savedGroups");
  Assert.equal(state.savedGroups[1].id, group2.id, "group2 is in savedGroups");

  ss.forgetSavedTabGroup(group1.id);
  state = ss.getCurrentState();

  Assert.equal(state.savedGroups.length, 1, "savedGroups has 1 group");
  Assert.equal(state.savedGroups[0].id, group2.id, "group2 is in savedGroups");

  ss.forgetSavedTabGroup(group2.id);
  await BrowserTestUtils.closeWindow(win);
  forgetClosedWindows();
});
