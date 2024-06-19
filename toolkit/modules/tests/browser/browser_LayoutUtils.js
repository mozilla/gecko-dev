/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_rectToBrowserRect() {
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "data:text/html;charset=utf-8,test"
  );

  SpecialPowers.addTaskImport(
    "LayoutUtils",
    "resource://gre/modules/LayoutUtils.sys.mjs"
  );

  // Convert (12, 34) in a content document coordinates into this browser window
  // coordinates.
  const positionInBrowser = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    () => {
      /* global LayoutUtils */
      return LayoutUtils.rectToTopLevelWidgetRect(content.window, {
        left: 12,
        top: 34,
        width: 0,
        height: 0,
      });
    }
  );

  // Dispatch a mousedown event on the browser window coordinates position to
  // see whether it's fired on the correct position in the content document.
  const mouseDownPromise = BrowserTestUtils.waitForContentEvent(
    tab.linkedBrowser,
    "mousedown",
    false,
    event => {
      dump(`mousedown on (${event.clientX}, ${event.clientY})`);
      return event.clientX == 12 && event.clientY == 34;
    }
  );

  // A workaround for bug 1743857.
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    await Promise.resolve();
  });

  EventUtils.synthesizeMouseAtPoint(
    positionInBrowser.x / window.devicePixelRatio,
    positionInBrowser.y / window.devicePixelRatio,
    { type: "mousedown", button: 1 }
  );
  await mouseDownPromise;

  Assert.ok(true, "LayoutUtils.rectToBrowserRect() works as expected");

  BrowserTestUtils.removeTab(tab);
});
