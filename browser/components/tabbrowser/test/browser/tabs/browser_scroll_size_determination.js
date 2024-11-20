/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that when opening a new window with vertical tabs turned
 * on/off, wheel events with DOM_DELTA_LINE deltaMode successfully
 * scroll the tabstrip.
 */
async function scrolling_works(useVerticalTabs, uiDensity) {
  info(`Testing UI density ${uiDensity}`);

  await SpecialPowers.pushPrefEnv({
    set: [
      ["sidebar.revamp", useVerticalTabs],
      ["sidebar.verticalTabs", useVerticalTabs],
    ],
  });

  let win = await BrowserTestUtils.openNewBrowserWindow();

  win.gUIDensity.update(win.gUIDensity[uiDensity]);

  await BrowserTestUtils.overflowTabs(null, win, {
    overflowAtStart: false,
    overflowTabFactor: 3,
  });

  await TestUtils.waitForCondition(() => {
    return Array.from(win.gBrowser.tabs).every(tab => tab._fullyOpen);
  });

  win.gBrowser.pinTab(win.gBrowser.tabs[0]);

  let firstScrollableTab = win.gBrowser.tabs[1];

  // Scroll back to start.
  win.gBrowser.selectedTab = firstScrollableTab;
  await TestUtils.waitForTick();
  await win.promiseDocumentFlushed(() => {});

  // Check we're scrolled so the first scrollable tab is at the top.
  let { arrowScrollbox } = win.gBrowser.tabContainer;
  let side = useVerticalTabs ? "top" : "left";
  let boxStart = arrowScrollbox.getBoundingClientRect()[side];
  let firstPoint = boxStart + 5;
  Assert.equal(
    gBrowser.tabs.indexOf(arrowScrollbox._elementFromPoint(firstPoint)),
    gBrowser.tabs.indexOf(firstScrollableTab),
    "First tab should be scrolled into view."
  );

  // Scroll.
  EventUtils.synthesizeWheel(
    arrowScrollbox,
    10,
    10,
    {
      wheel: true,
      deltaY: 1,
      deltaMode: WheelEvent.DOM_DELTA_LINE,
    },
    win
  );

  // Check that some other tab is scrolled into view.
  try {
    await TestUtils.waitForCondition(() => {
      return arrowScrollbox._elementFromPoint(firstPoint) != firstScrollableTab;
    });
  } catch (ex) {
    Assert.ok(false, `Failed to see scroll, error: ${ex}`);
  }
  Assert.notEqual(
    win.gBrowser.tabs.indexOf(arrowScrollbox._elementFromPoint(firstPoint)),
    win.gBrowser.tabs.indexOf(firstScrollableTab),
    "First tab should be scrolled out of view."
  );

  await SpecialPowers.popPrefEnv();

  await BrowserTestUtils.closeWindow(win);
}

add_task(async function test_vertical_scroll() {
  for (let density of ["MODE_NORMAL", "MODE_COMPACT", "MODE_TOUCH"]) {
    await scrolling_works(true, density);
  }
});

add_task(async function test_horizontal_scroll() {
  for (let density of ["MODE_NORMAL", "MODE_COMPACT", "MODE_TOUCH"]) {
    await scrolling_works(false, density);
  }
});
