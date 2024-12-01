/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This test is based on
     https://searchfox.org/mozilla-central/rev/25d26b0a62cc5bb4aa3bb90a11f3b0b7c52859c4/gfx/layers/apz/test/mochitest/browser_test_position_sticky.js
*/

"use strict";

requestLongerTimeout(2);

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/gfx/layers/apz/test/mochitest/apz_test_utils.js",
  this
);

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/gfx/layers/apz/test/mochitest/apz_test_native_event_utils.js",
  this
);

async function runOneTest(filename) {
  function httpURL(sfilename) {
    let chromeURL = getRootDirectory(gTestPath) + sfilename;
    return chromeURL.replace(
      "chrome://mochitests/content/",
      "http://mochi.test:8888/"
    );
  }

  const url = httpURL(filename);
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  const { rect } = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    async () => {
      const container = content.document.getElementById("container");

      // Get the area in the screen coords where the position:sticky element is.
      let containerRect = container.getBoundingClientRect();
      containerRect.x += content.window.mozInnerScreenX;
      containerRect.y += content.window.mozInnerScreenY;

      await content.wrappedJSObject.promiseApzFlushedRepaints();
      await content.wrappedJSObject.waitUntilApzStable();

      let w = {},
        h = {};
      SpecialPowers.DOMWindowUtils.getScrollbarSizes(
        content.document.documentElement,
        w,
        h
      );

      // Reduce the scrollbar width from the sticky area.
      containerRect.width -= w.value;
      return {
        rect: containerRect,
      };
    }
  );

  const reference = await getSnapshot({
    x: rect.x,
    y: rect.y,
    width: rect.width,
    height: rect.height,
  });

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    // start the toggling the position on a setInterval which triggers the bug
    content.wrappedJSObject.doTest();
  });

  // have to try many times to capture the bug
  for (let i = 0; i < 100; i++) {
    let snapshot = await getSnapshot({
      x: rect.x,
      y: rect.y,
      width: rect.width,
      height: rect.height,
    });

    is(snapshot, reference, "should be same " + filename);
  }

  BrowserTestUtils.removeTab(tab);
}

add_task(async () => {
  await runOneTest("helper_oopif_reconstruct.html");
  await runOneTest("helper_oopif_reconstruct_nested.html");
});
