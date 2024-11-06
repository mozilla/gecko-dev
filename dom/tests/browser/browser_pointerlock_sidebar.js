/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const TEST_URL =
  "data:text/html,<body><div style='width: 100px; height: 100px; background-color: red;'></div></body>";

function requestPointerLockRemote(aRemote) {
  return SpecialPowers.spawn(aRemote, [], function () {
    return new Promise(resolve => {
      content.document.addEventListener(
        "pointerlockchange",
        _e => {
          info(`Received pointerlockchange event`);
          resolve();
        },
        { once: true }
      );

      SpecialPowers.wrap(content.document).notifyUserGestureActivation();
      content.document.body.requestPointerLock();
    });
  });
}

function exitPointerLockRemote(aRemote) {
  return SpecialPowers.spawn(aRemote, [], function () {
    return new Promise(resolve => {
      if (!content.document.pointerLockElement) {
        resolve();
        return;
      }

      content.document.addEventListener(
        "pointerlockchange",
        _e => {
          info(`Received pointerlockchange event`);
          resolve();
        },
        { once: true }
      );
      content.document.exitPointerLock();
    });
  });
}

function isPointerLockedRemote(aRemote) {
  return SpecialPowers.spawn(aRemote, [], function () {
    return !!content.document.pointerLockElement;
  });
}

add_task(async function test_pointerlock_close_sidebar() {
  info("Open new browser window");
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const tab = await BrowserTestUtils.openNewForegroundTab(
    win.gBrowser,
    TEST_URL
  );

  info("Open sidebar");
  const sidebar = win.document.getElementById("sidebar");
  let loadPromise = BrowserTestUtils.waitForEvent(sidebar, "load", true);
  await win.SidebarController.show("viewBookmarksSidebar");
  await loadPromise;

  info("Switch focus back to tab");
  const browser = tab.linkedBrowser;
  await SimpleTest.promiseFocus(browser);

  info("Request PointerLock");
  await requestPointerLockRemote(browser);

  info("Close sidebar");
  win.SidebarController.hide();
  await new Promise(resolve => SimpleTest.executeSoon(resolve));
  ok(await isPointerLockedRemote(browser), "Pointer should still be locked");

  info("Exit PointerLock");
  await exitPointerLockRemote(browser);

  // Close opened window
  info("Close new browser window");
  await BrowserTestUtils.closeWindow(win);
});
