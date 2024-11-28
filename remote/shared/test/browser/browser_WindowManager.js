/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { windowManager } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/WindowManager.sys.mjs"
);

add_task(async function test_windows() {
  const win1 = await BrowserTestUtils.openNewBrowserWindow();
  const win2 = await BrowserTestUtils.openNewBrowserWindow();
  const win3 = await BrowserTestUtils.openNewBrowserWindow();

  const expectedWindows = [gBrowser.ownerGlobal, win1, win2, win3];

  try {
    is(
      windowManager.windows.length,
      5,
      "All browser windows and the Mochikit harness window were returned"
    );
    ok(
      expectedWindows.every(win => windowManager.windows.includes(win)),
      "Expected windows were returned"
    );
  } finally {
    await BrowserTestUtils.closeWindow(win3);
    await BrowserTestUtils.closeWindow(win2);
    await BrowserTestUtils.closeWindow(win1);
  }
});
