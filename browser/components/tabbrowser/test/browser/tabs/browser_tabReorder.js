/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function () {
  let initialTabsLength = gBrowser.tabs.length;

  let newTab1 = (gBrowser.selectedTab = BrowserTestUtils.addTab(
    gBrowser,
    "about:robots",
    { skipAnimation: true }
  ));
  let newTab2 = (gBrowser.selectedTab = BrowserTestUtils.addTab(
    gBrowser,
    "about:about",
    { skipAnimation: true }
  ));
  let newTab3 = (gBrowser.selectedTab = BrowserTestUtils.addTab(
    gBrowser,
    "about:config",
    { skipAnimation: true }
  ));
  registerCleanupFunction(function () {
    while (gBrowser.tabs.length > initialTabsLength) {
      gBrowser.removeTab(gBrowser.tabs[initialTabsLength]);
    }
  });

  is(gBrowser.tabs.length, initialTabsLength + 3, "new tabs are opened");
  is(newTab1._tPos, initialTabsLength, "newTab1 position is correct");
  is(newTab2._tPos, initialTabsLength + 1, "newTab2 position is correct");
  is(newTab3._tPos, initialTabsLength + 2, "newTab3 position is correct");

  await dragAndDrop(newTab1, newTab2, false);
  is(gBrowser.tabs.length, initialTabsLength + 3, "tabs are still there");
  is(newTab2._tPos, initialTabsLength, "newTab2 and newTab1 are swapped");
  is(newTab1._tPos, initialTabsLength + 1, "newTab1 and newTab2 are swapped");
  is(newTab3._tPos, initialTabsLength + 2, "newTab3 stays same place");

  await dragAndDrop(newTab2, newTab1, true);
  is(gBrowser.tabs.length, initialTabsLength + 4, "a tab is duplicated");
  is(newTab2._tPos, initialTabsLength, "newTab2 stays same place");
  is(newTab1._tPos, initialTabsLength + 1, "newTab1 stays same place");
  is(
    newTab3._tPos,
    initialTabsLength + 3,
    "a new tab is inserted before newTab3"
  );
});
