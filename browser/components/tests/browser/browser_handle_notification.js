/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function createCmdLine(tag, action, state) {
  return Cu.createCommandLine(
    [
      "--notification-windowsTag",
      tag,
      "--notification-windowsAction",
      JSON.stringify(action),
    ],
    null,
    state
  );
}

function runCmdLine(cmdLine) {
  let cmdLineHandler = Cc["@mozilla.org/browser/final-clh;1"].getService(
    Ci.nsICommandLineHandler
  );
  cmdLineHandler.handle(cmdLine);
}

function simulateNotificationClickWithExistingWindow(action) {
  let cmdLine = createCmdLine(
    "dummyTag",
    action,
    Ci.nsICommandLine.STATE_REMOTE_AUTO
  );
  runCmdLine(cmdLine);
}

function simulateNotificationClickWithNewWindow(action) {
  let cmdLine = createCmdLine(
    "dummyTag",
    action,
    Ci.nsICommandLine.STATE_INITIAL_LAUNCH
  );
  runCmdLine(cmdLine);
}

add_task(async function test_basic() {
  let newTabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "https://example.com/"
  );

  simulateNotificationClickWithExistingWindow({
    action: "",
    origin: "https://example.com",
  });

  let newTab = await newTabPromise;
  ok(newTab, "New tab should be opened.");
  BrowserTestUtils.removeTab(newTab);
});

// launchUrl was used pre-140, we can remove it when we are confident enough
// that there's no old notification with launchUrl lying around anymore
add_task(async function test_legacy_launchUrl() {
  let newTabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "https://example.com/"
  );

  simulateNotificationClickWithExistingWindow({
    action: "",
    launchUrl: "https://example.com",
  });

  let newTab = await newTabPromise;
  ok(newTab, "New tab should be opened.");
  BrowserTestUtils.removeTab(newTab);
});

add_task(async function test_invalid_origin_with_path() {
  let newTabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "https://example.com/"
  );

  simulateNotificationClickWithExistingWindow({
    action: "",
    origin: "https://example.com/example/",
  });

  let newTab = await newTabPromise;
  ok(newTab, "New tab should be opened.");
  BrowserTestUtils.removeTab(newTab);
});

add_task(async function test_user_context() {
  let newTabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "https://example.com/"
  );

  simulateNotificationClickWithExistingWindow({
    action: "",
    origin: "https://example.com^userContextId=1",
  });

  let newTab = await newTabPromise;
  registerCleanupFunction(() => BrowserTestUtils.removeTab(newTab));
  ok(newTab, "New tab should be opened.");

  // TODO(krosylight): We want to make sure this opens on the right container.
  // See bug 1945501.
  is(newTab.userContextId, 0, "The default user context ID is used (for now).");
});

add_task(async function test_basic_initial_load() {
  let newWinPromise = BrowserTestUtils.waitForNewWindow({
    url: "https://example.com/",
    anyWindow: true,
  });

  simulateNotificationClickWithNewWindow({
    action: "",
    origin: "https://example.com",
  });

  let newWin = await newWinPromise;
  ok(newWin, "New window should be opened.");
  BrowserTestUtils.closeWindow(newWin);
});
