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
    ],
  });
});

async function runTest(aTestFile) {
  // Open the given file with chrome:// scheme.
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    getRootDirectory(gTestPath) + aTestFile
  );

  // Make sure the document gets loaded in the parent process.
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    Assert.ok(SpecialPowers.isMainProcess());
  });

  await promiseApzFlushedRepaints();
  await waitUntilApzStable();

  const selectChangePromise = SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    return new Promise(resolve => {
      content.document
        .querySelector("select")
        .addEventListener("change", () => resolve());
    });
  });
  // Wait to ensure the above event listener has been registered.
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    return new Promise(resolve => resolve());
  });

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

  // Do a mouse click on the middle of the popup options.
  const popupRect = selectPopup.getBoundingClientRect();
  await synthesizeNativeMouseEventWithAPZ({
    type: "click",
    target: selectPopup,
    offsetX: popupRect.width / 2,
    offsetY: popupRect.height / 2,
  });

  await selectChangePromise;
  ok(true, "clicking on poped up element works");

  BrowserTestUtils.removeTab(tab);
}

add_task(async () => {
  await runTest("helper_popup_menu_in_parent_process-1.html");
});

add_task(async () => {
  await runTest("helper_popup_menu_in_parent_process-2.html");
});
