/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

ChromeUtils.defineESModuleGetters(this, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
});

const BASE_URL = "https://example.com/browser/browser/modules/test/browser/";

add_task(async function createTaskBarTabTest() {
  let createTaskbarTabButton = window.document.getElementById(
    "taskbar-tabs-button"
  );
  ok(createTaskbarTabButton, "Taskbar tab page action button should exist");

  is(
    createTaskbarTabButton.hidden,
    false,
    "Taskbar tab page action button should not be hidden"
  );

  await BrowserTestUtils.openNewForegroundTab({
    gBrowser: window.gBrowser,
    url: BASE_URL + "dummy_page.html",
  });

  let newWinPromise = BrowserTestUtils.waitForNewWindow();

  // Create a taskbar tab window
  const clickEvent = new PointerEvent("click", {
    view: window,
  });
  createTaskbarTabButton.dispatchEvent(clickEvent);

  let taskbarTabWindow = await newWinPromise;

  ok(
    taskbarTabWindow.document.documentElement.hasAttribute("taskbartab"),
    "The window HTML should have a taskbartab attribute"
  );

  is(
    BrowserWindowTracker.getAllVisibleTabs().length,
    2,
    "The number of existing tabs should be two"
  );

  await BrowserTestUtils.closeWindow(taskbarTabWindow);
});
