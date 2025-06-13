/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BASE_URL =
  "https://example.com/browser/browser/components/taskbartabs/test/browser/dummy_page.html";

ChromeUtils.defineESModuleGetters(this, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
  FileTestUtils: "resource://testing-common/FileTestUtils.sys.mjs",
  sinon: "resource://testing-common/Sinon.sys.mjs",
  TaskbarTabs: "resource:///modules/taskbartabs/TaskbarTabs.sys.mjs",
  TaskbarTabsPin: "resource:///modules/taskbartabs/TaskbarTabsPin.sys.mjs",
});

sinon.stub(TaskbarTabsPin, "pinTaskbarTab");
sinon.stub(TaskbarTabsPin, "unpinTaskbarTab");

registerCleanupFunction(async () => {
  sinon.restore();
  await TaskbarTabs.resetForTests();
});

async function browserPageAction(win) {
  let pageAction = win.document.getElementById("taskbar-tabs-button");
  ok(pageAction, "Taskbar tab page action button should exist.");

  is(
    pageAction.hidden,
    false,
    "Taskbar tab page action button should not be hidden."
  );

  let newWinPromise = BrowserTestUtils.waitForNewWindow();

  const clickEvent = new PointerEvent("click", {
    view: win,
  });
  pageAction.dispatchEvent(clickEvent);

  let taskbarTabWindow = await newWinPromise;

  ok(
    taskbarTabWindow.document.documentElement.hasAttribute("taskbartab"),
    "The window HTML should have a taskbartab attribute."
  );

  return taskbarTabWindow;
}

async function taskbarTabsPageAction(win, destWin) {
  let pageAction = win.document.getElementById("taskbar-tabs-button");
  ok(pageAction, "Taskbar tab page action button should exist.");

  is(
    pageAction.hidden,
    false,
    "Taskbar tab page action button should not be hidden."
  );

  let closeWinPromise = BrowserTestUtils.windowClosed(win);

  const tabOpenPromise = BrowserTestUtils.waitForEvent(
    destWin.gBrowser.tabContainer,
    "TabOpen"
  );

  const clickEvent = new PointerEvent("click", {
    view: win,
  });
  pageAction.dispatchEvent(clickEvent);

  await closeWinPromise;
  let tab = (await tabOpenPromise).target;

  is(
    tab.ownerGlobal,
    destWin,
    "Shoud've reverted back to secondWin, as it is most recently focused"
  );

  ok(
    !tab.ownerDocument.documentElement.hasAttribute("taskbartab"),
    "The window HTML should not have a taskbartab attribute."
  );
  return tab;
}

add_task(async function test_taskbar_tabs_page_action() {
  // Open first Taskbar Tab via page action.
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser: window.gBrowser,
    url: BASE_URL,
  });
  let taskbarTabsWindow1 = await browserPageAction(window);

  is(
    BrowserWindowTracker.getAllVisibleTabs().length,
    2,
    "The number of existing tabs should be two."
  );

  ok(
    TaskbarTabsPin.pinTaskbarTab.called,
    `Pin to taskbar should have been called.`
  );

  // Open second Taskbar Tab via page action.
  sinon.resetHistory();
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser: window.gBrowser,
    url: BASE_URL,
  });
  let taskbarTabsWindow2 = await browserPageAction(window);

  is(
    BrowserWindowTracker.getAllVisibleTabs().length,
    3,
    "The number of existing tabs should be three."
  );

  ok(
    !TaskbarTabsPin.pinTaskbarTab.called,
    `Pin to taskbar should not have been called.`
  );

  // Close first window via page action.
  sinon.resetHistory();
  let tab1 = await taskbarTabsPageAction(taskbarTabsWindow1, window);
  ok(
    !TaskbarTabsPin.unpinTaskbarTab.called,
    `Unpin from taskbar should not have been called.`
  );

  // Close second window via page action.
  sinon.resetHistory();
  let tab2 = await taskbarTabsPageAction(taskbarTabsWindow2, window);
  ok(
    TaskbarTabsPin.unpinTaskbarTab.called,
    `Unpin from taskbar should have been called.`
  );

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});

// Browser tests start with a window1. Convert a tab in this window into a web
// app. Then open another window, window2. Click "revert" on the web app, the
// tab should be back to window1.
add_task(async function revertToOriginal() {
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser: window.gBrowser,
    url: BASE_URL,
  });

  let taskbarTabWindow = await browserPageAction(window);
  is(
    window.gBrowser.openTabs.length,
    1,
    "The original window should have one tab"
  );
  let secondWindow = await BrowserTestUtils.openNewBrowserWindow();

  // Revert back to original window
  let tab = await taskbarTabsPageAction(taskbarTabWindow, window);

  await BrowserTestUtils.removeTab(tab);
  await BrowserTestUtils.closeWindow(secondWindow);
});

// If the origin window of a web app is closed, then clicking revert should put
// the tab into the most recently used window.
add_task(async function revertToMostRecent() {
  let firstWin = await BrowserTestUtils.openNewBrowserWindow();

  await BrowserTestUtils.openNewForegroundTab({
    gBrowser: firstWin.gBrowser,
    url: BASE_URL,
  });
  let taskbarTabWindow = await browserPageAction(firstWin);

  let secondWin = await BrowserTestUtils.openNewBrowserWindow();
  let thirdWin = await BrowserTestUtils.openNewBrowserWindow();

  await BrowserTestUtils.closeWindow(firstWin);
  secondWin.focus();

  // Revert back to regular window
  await taskbarTabsPageAction(taskbarTabWindow, secondWin);

  await BrowserTestUtils.closeWindow(secondWin);
  await BrowserTestUtils.closeWindow(thirdWin);
});
