/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochikit/content/tests/SimpleTest/paint_listener.js",
  this
);

Services.scriptloader.loadSubScript(
  new URL("apz_test_utils.js", gTestPath).href,
  this
);

Services.scriptloader.loadSubScript(
  new URL("apz_test_native_event_utils.js", gTestPath).href,
  this
);

/* import-globals-from helper_browser_test_utils.js */
// For openSelectPopup.
Services.scriptloader.loadSubScript(
  new URL("helper_browser_test_utils.js", gTestPath).href,
  this
);

const { UrlbarTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/UrlbarTestUtils.sys.mjs"
);

// Cleanup for paint_listener.js.
add_task(() => {
  registerCleanupFunction(() => {
    delete window.waitForAllPaintsFlushed;
    delete window.waitForAllPaints;
    delete window.promiseAllPaintsDone;
  });
});

// Setup preferences.
add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["apz.popups.enabled", true],
      ["apz.popups_without_remote.enabled", true],
      ["test.events.async.enabled", true],
    ],
  });
});

add_task(async () => {
  // Open the given file with chrome:// scheme.
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    getRootDirectory(gTestPath) + "helper_popup_menu_in_parent_process-2.html"
  );

  // Make sure the document gets loaded in the parent process.
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    Assert.ok(SpecialPowers.isMainProcess());
  });

  await promiseApzFlushedRepaints();
  await waitUntilApzStable();

  // Focus to the select element. This stuff is necessary for `openSelectPopup()`
  // since the function is triggered on the focused element.
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    const select = content.document.querySelector("select");
    const focusPromise = new Promise(resolve => {
      select.addEventListener("focus", resolve, { once: true });
    });
    select.focus();
    await focusPromise;
  });

  // Open the select popup.
  const selectPopup = await openSelectPopup();

  // And make sure APZ is ready there.
  await promiseApzFlushedRepaints(selectPopup);

  // Send a bunch of mousemove events on the popup window.
  // NOTE: We need to use nsIDOMWindowUtils.sendMouseEvent directly rather than
  // synthesizeNativeMouseEventWithAPZ since synthesizeNativeMouseEventWithAPZ
  // causes a paint which prevents the assertion in FocusState::IsCurrent().
  const utils = SpecialPowers.DOMWindowUtils;
  for (let x = 0; x < 20; x++) {
    await new Promise(resolve => {
      utils.sendNativeMouseEvent(
        10 + x,
        10,
        utils.NATIVE_MOUSE_MESSAGE_MOVE,
        0 /* button */,
        0 /* modifiers */,
        selectPopup,
        resolve
      );
    });
  }

  // Do a key down on popup.
  info("pressing a key");
  EventUtils.synthesizeKey("Key_Escape");

  BrowserTestUtils.removeTab(tab);
});
