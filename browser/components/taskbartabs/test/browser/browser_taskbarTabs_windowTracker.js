/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

ChromeUtils.defineESModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
});

add_task(async function testGetTopWindow() {
  let win = await openTaskbarTabWindow();
  win.focus();

  let topWindow = BrowserWindowTracker.getTopWindow({ allowTaskbarTabs: true });

  ok(
    topWindow.document.documentElement.hasAttribute("taskbartab"),
    "The taskbar tab window should've been fetched"
  );

  topWindow = BrowserWindowTracker.getTopWindow();

  ok(
    !topWindow.document.documentElement.hasAttribute("taskbartab"),
    "The regular Firefox window should've been fetched"
  );

  await BrowserTestUtils.closeWindow(win);
});
