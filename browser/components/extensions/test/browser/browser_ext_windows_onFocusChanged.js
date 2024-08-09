/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

async function openNonBrowserWindow() {
  info("Opening non-browser window");
  const win = Services.ww.openWindow(
    window,
    "chrome://extensions/content/dummy.xhtml",
    "_blank",
    "chrome,dialog=no,all",
    null
  );
  await SimpleTest.promiseFocus(win);
  info("Opened and focused non-browser window");
  return win;
}

function simulateFocusChanged() {
  // Simulate onFocusChanged event. Ordinarily it is fired when "blur" or
  // "focus" events are triggered on a browser window. When the "blur" event is
  // fired, any other window (including non-browser) windows could have been
  // focused in the meantime.  To make the test less susceptible to race
  // conditions, we just trigger a fake "blur" event while a non-browser window
  // has focus.
  const {
    Management: {
      global: { windowTracker },
    },
  } = ChromeUtils.importESModule("resource://gre/modules/Extension.sys.mjs");

  ok(windowTracker._listeners.has("blur"), "windowTracker has blur listener");
  let blurListeners = windowTracker._listeners.get("blur");
  is(blurListeners.size, 1, "windowTracker has one blur listener");
  let blurListenerFromOnFocusChanged = blurListeners.values().next().value;
  info(`Simulating onFocusChanged listener: ${blurListenerFromOnFocusChanged}`);
  blurListenerFromOnFocusChanged();
}

// Regression test for bug 1634240 : When onFocusChanged is fired, and a
// non-browser window has focus when the focus() or blur() event is processed,
// then onFocusChanged should not report that non-browser window.
add_task(async function test_onFocusChanged_when_open_non_browser_window() {
  function background() {
    let winIdPromise;
    let seenWindowId = false;
    browser.windows.onFocusChanged.addListener(async windowId => {
      let initialWindowId = await winIdPromise;
      if (!seenWindowId) {
        if (windowId === -1) {
          // -1 may fire when transitioning between windows.
          browser.test.log("Ignore windowId -1 from switch to initial window");
          return;
        }
        browser.test.assertEq(initialWindowId, windowId, "Got initial window");
        seenWindowId = true;
      }
      browser.test.log(`onFocusChange: windowId=${windowId}`);
      browser.test.sendMessage("onFocusChanged", windowId);
    });
    browser.test.onMessage.addListener(async msg => {
      browser.test.assertEq("closeWindow", msg, "expected message");
      await browser.windows.remove(await winIdPromise);
      browser.test.sendMessage("done");
    });
    winIdPromise = browser.windows.create({}).then(win => win.id);
    winIdPromise.then(windowId =>
      browser.test.sendMessage("initialWindow", windowId)
    );
  }
  let extension = ExtensionTestUtils.loadExtension({ background });
  await extension.startup();
  let [initialWindowId, focusedWindowId] = await Promise.all([
    extension.awaitMessage("initialWindow"),
    extension.awaitMessage("onFocusChanged"),
  ]);
  is(initialWindowId, focusedWindowId, "New browser window has focus");

  const win = await openNonBrowserWindow();

  let newFocusedWindowId = await extension.awaitMessage("onFocusChanged");
  is(newFocusedWindowId, -1, "After losing focus, windowId is -1");
  // ^ Note: This test on its own is not enough to prove that the non-browser
  // window has windowId -1, because some platforms may dispatch onFocusChanged
  // with windowId -1 when the focus changes to anything else.
  // Regardless of the reason, the above should always be -1. A deterministic
  // version of the test is below, at
  // test_simulate_onFocusChanged_with_non_browser_window.

  extension.sendMessage("closeWindow");
  await extension.awaitMessage("done");

  await extension.unload();

  win.close();
});

// Regression test for bug 1634240 : When onFocusChanged is fired, and a
// non-browser window has focus when the focus() or blur() event is processed,
// then onFocusChanged should not report that non-browser window.
// This is the simulated, deterministic version of the test task above,
// test_onFocusChanged_when_open_non_browser_window.
add_task(async function test_simulate_onFocusChanged_with_non_browser_window() {
  const win = await openNonBrowserWindow();

  function background() {
    let count = 0;
    browser.windows.onFocusChanged.addListener(windowId => {
      browser.test.assertEq(1, ++count, "expect one fake onFocusChanged event");
      browser.test.assertEq(
        -1,
        windowId,
        "When non-browser window is focused, windowId is -1."
      );
      browser.test.sendMessage("done");
    });
  }
  let extension = ExtensionTestUtils.loadExtension({ background });
  await extension.startup();

  // Sanity check: Confirm that the non-browser window was focused, using the
  // same implementation that ext-windows.js uses to look up the currently
  // focused window in the onFocusChanged implementation.
  is(Services.focus.activeWindow, win, "Non-browser window is focused");

  // Now fake the onFocusChanged event. Because the onFocusChanged event had
  // not been dispatched before, the value of lastOnFocusChangedWindowId in
  // ext-windows.js will change (from undefined) and windows.onFocusChanged
  // should fire with windowId -1. Because a non-browser window was focused at
  // the time of event dispatch, windowId should be -1 (and not a "valid"
  // windowId, unlike what was seen in bug 1634240 - this is a regression test).
  simulateFocusChanged();

  info("Waiting for onFocusChanged to be triggered");
  await extension.awaitMessage("done");
  await extension.unload();

  win.close();
});
