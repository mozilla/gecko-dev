/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_removeAllButSingleTab() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  let tab1 = await addTabTo(win.gBrowser);
  let tab2 = await addTabTo(win.gBrowser);
  let tab3 = await addTabTo(win.gBrowser);

  Assert.equal(
    win.gBrowser.tabs.length,
    4,
    "should be 1 new tab from window + 3 added tabs from test"
  );

  win.gBrowser.removeAllTabsBut(tab2);

  await TestUtils.waitForCondition(
    () => win.gBrowser.tabs.length == 1,
    "waiting for other tabs to close"
  );

  Assert.ok(
    !win.gBrowser.tabs.some(tab => tab == tab1),
    "tab1 should have been closed"
  );
  Assert.ok(
    win.gBrowser.tabs.some(tab => tab == tab2),
    "tab2 should still be present"
  );
  Assert.ok(
    !win.gBrowser.tabs.some(tab => tab == tab3),
    "tab3 should have been closed"
  );

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_removesAllButMultiselectedTabs() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  let tab1 = await addTabTo(win.gBrowser);
  let tab2 = await addTabTo(win.gBrowser);
  let tab3 = await addTabTo(win.gBrowser);
  let tab4 = await addTabTo(win.gBrowser);
  let tab5 = await addTabTo(win.gBrowser);

  Assert.equal(
    win.gBrowser.tabs.length,
    6,
    "should be 1 new tab from window + 5 added tabs from test"
  );

  win.gBrowser.selectedTabs = [tab2, tab4];

  win.gBrowser.removeAllTabsBut(tab2);

  await TestUtils.waitForCondition(
    () => win.gBrowser.tabs.length == 2,
    "waiting for other tabs to close"
  );

  Assert.ok(
    !win.gBrowser.tabs.some(tab => tab == tab1),
    "tab1 should have been closed"
  );
  Assert.ok(
    win.gBrowser.tabs.some(tab => tab == tab2),
    "tab2 should still be present"
  );
  Assert.ok(
    !win.gBrowser.tabs.some(tab => tab == tab3),
    "tab3 should have been closed"
  );
  Assert.ok(
    win.gBrowser.tabs.some(tab => tab == tab4),
    "tab4 should still be present"
  );
  Assert.ok(
    !win.gBrowser.tabs.some(tab => tab == tab5),
    "tab5 should have been closed"
  );

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_removesAllIncludingTabGroups() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  let tab1 = await addTabTo(win.gBrowser);
  let tab2 = await addTabTo(win.gBrowser);
  let tab3 = await addTabTo(win.gBrowser);
  let tab4 = await addTabTo(win.gBrowser);

  win.gBrowser.addTabGroup([tab3, tab4]);

  Assert.equal(
    win.gBrowser.tabs.length,
    5,
    "should be 1 new tab from window + 5 added tabs from test"
  );
  Assert.equal(win.gBrowser.tabGroups.length, 1, "should be 1 tab group");

  win.gBrowser.removeAllTabsBut(tab2);

  await TestUtils.waitForCondition(
    () => win.gBrowser.tabs.length == 1,
    "waiting for other tabs to close"
  );

  Assert.ok(
    !win.gBrowser.tabs.some(tab => tab == tab1),
    "tab1 should have been closed"
  );
  Assert.ok(
    win.gBrowser.tabs.some(tab => tab == tab2),
    "tab2 should still be present"
  );
  Assert.ok(
    !win.gBrowser.tabs.some(tab => tab == tab3),
    "tab3 should have been closed"
  );
  Assert.ok(
    !win.gBrowser.tabs.some(tab => tab == tab4),
    "tab4 should have been closed"
  );
  Assert.equal(
    win.gBrowser.tabGroups.length,
    0,
    "tab group should have been deleted"
  );

  await BrowserTestUtils.closeWindow(win);
});
