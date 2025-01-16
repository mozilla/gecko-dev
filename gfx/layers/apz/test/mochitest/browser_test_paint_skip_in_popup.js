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
      ["apz.test.logging_enabled", true],
      ["layout.scroll.disable-pixel-alignment", true],
    ],
  });
});

add_task(async () => {
  function httpURL(filename) {
    let chromeURL = getRootDirectory(gTestPath) + filename;
    return chromeURL.replace(
      "chrome://mochitests/content/",
      "http://mochi.test:8888/"
    );
  }
  const url = httpURL("helper_paint_skip_in_popup.html");
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    await content.wrappedJSObject.promiseApzFlushedRepaints();
    await content.wrappedJSObject.waitUntilApzStable();
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

  const arrowscrollbox = selectPopup.shadowRoot.querySelector("arrowscrollbox");
  ok(arrowscrollbox, "There's <arrowscrollbox> inside the popup");

  const scrollbox = arrowscrollbox.shadowRoot.querySelector("scrollbox");
  ok(scrollbox, "There's <scrollbox> inside the popup");

  // Scroll down as much as possible to get the max scroll offset.
  scrollbox.scrollTo(0, 100000);
  const scrollMax = scrollbox.scrollTop;
  ok(scrollMax, "The max scroll offset should not be zero");

  // Restore the scroll offset.
  // Note that this restoring needs to be done in the same paint where the above scrollTo happened so that
  // APZ will never sample the max scroll offset at this moment.
  scrollbox.scrollTo(0, 0);
  await promiseApzFlushedRepaints(selectPopup);

  // Now scroll to a position which is close to the bottom.
  scrollbox.scrollBy(0, scrollMax - 10);
  await promiseApzFlushedRepaints(selectPopup);

  // Try to scroll to the bottom with a `scrollBy` call, even if paint-skip
  // is enabled, this scroll operation should be reflected to APZ.
  scrollbox.scrollBy(0, 10);

  // Wait a bit to make sure that APZ has sampled the new scroll offset.
  await promiseApzFlushedRepaints(selectPopup);
  await promiseApzFlushedRepaints(selectPopup);

  is(
    scrollbox.scrollTop,
    scrollMax,
    `The scroll offset: ${scrollbox.scrollTop} on the main-thread should be ${scrollMax}`
  );

  const sampledData = collectSampledScrollOffsets(scrollbox, selectPopup);
  ok(sampledData.length, "There should be at least one collected offsets");
  ok(
    sampledData.some(
      data => SpecialPowers.wrap(data).scrollOffsetY == scrollMax
    ),
    `There should be ${scrollMax} in [${sampledData.map(data => SpecialPowers.wrap(data).scrollOffsetY)}]`
  );

  BrowserTestUtils.removeTab(tab);
});
