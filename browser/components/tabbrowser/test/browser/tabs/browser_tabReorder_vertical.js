/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_setup(() =>
  SpecialPowers.pushPrefEnv({
    set: [
      ["sidebar.revamp", true],
      ["sidebar.verticalTabs", true],
    ],
  })
);

add_task(async function () {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  let initialTabsLength = win.gBrowser.tabs.length;

  let newTab1 = (win.gBrowser.selectedTab = BrowserTestUtils.addTab(
    win.gBrowser,
    "about:robots",
    { skipAnimation: true }
  ));
  let newTab2 = (win.gBrowser.selectedTab = BrowserTestUtils.addTab(
    win.gBrowser,
    "about:about",
    { skipAnimation: true }
  ));
  let newTab3 = (win.gBrowser.selectedTab = BrowserTestUtils.addTab(
    win.gBrowser,
    "about:config",
    { skipAnimation: true }
  ));

  registerCleanupFunction(function () {
    while (win.gBrowser.tabs.length > initialTabsLength) {
      win.gBrowser.removeTab(win.gBrowser.tabs[initialTabsLength]);
    }
  });

  is(win.gBrowser.tabs.length, initialTabsLength + 3, "new tabs are opened");
  is(
    win.gBrowser.tabs[initialTabsLength],
    newTab1,
    "newTab1 position is correct"
  );
  is(
    win.gBrowser.tabs[initialTabsLength + 1],
    newTab2,
    "newTab2 position is correct"
  );
  is(
    win.gBrowser.tabs[initialTabsLength + 2],
    newTab3,
    "newTab3 position is correct"
  );

  await dragAndDrop(newTab1, newTab2, false, win, true, win);
  is(win.gBrowser.tabs.length, initialTabsLength + 3, "tabs are still there");
  is(
    win.gBrowser.tabs[initialTabsLength],
    newTab2,
    "newTab2 and newTab1 are swapped"
  );
  is(
    win.gBrowser.tabs[initialTabsLength + 1],
    newTab1,
    "newTab1 and newTab2 are swapped"
  );
  is(
    win.gBrowser.tabs[initialTabsLength + 2],
    newTab3,
    "newTab3 stays same place"
  );

  await dragAndDrop(newTab2, newTab1, true, win, true, win);
  is(win.gBrowser.tabs.length, initialTabsLength + 4, "a tab is duplicated");
  is(win.gBrowser.tabs[initialTabsLength], newTab2, "newTab2 stays same place");
  is(
    win.gBrowser.tabs[initialTabsLength + 1],
    newTab1,
    "newTab1 stays same place"
  );
  is(
    win.gBrowser.tabs[initialTabsLength + 3],
    newTab3,
    "a new tab is inserted before newTab3"
  );

  // clean up extra tabs
  while (win.gBrowser.tabs.length > 1) {
    BrowserTestUtils.removeTab(win.gBrowser.tabs.at(-1));
  }
  await BrowserTestUtils.closeWindow(win);
});
