/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/gfx/layers/apz/test/mochitest/apz_test_utils.js",
  this
);

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/gfx/layers/apz/test/mochitest/apz_test_native_event_utils.js",
  this
);

Services.scriptloader.loadSubScript(
  "chrome://mochikit/content/tests/SimpleTest/WindowSnapshot.js",
  this
);

// usesFailurePatterns is defined in SimpleTest.js and used in
// WindowSnapshot.js but SimpleTest.js can't be loaded in browser mochitests
// (there's an equivalent script named browser-test.js for browser mochitests),
// so we define usesFailurePatterns which just returns false to make functions
// in WindowSnapshot.js work without loading SimpleTest.js.
function usesFailurePatterns() {
  return false;
}

async function convertDataURLtoCanvas(aDataURL, aWidth, aHeight) {
  const canvas = document.createElementNS(
    "http://www.w3.org/1999/xhtml",
    "canvas"
  );
  canvas.width = aWidth;
  canvas.height = aHeight;
  const image = new Image();
  const ctx = canvas.getContext("2d");
  const loadPromise = new Promise(resolve =>
    image.addEventListener("load", resolve)
  );
  image.src = aDataURL;
  await loadPromise;
  ctx.drawImage(image, 0, 0);
  return canvas;
}

add_task(async () => {
  function httpURL(filename) {
    let chromeURL = getRootDirectory(gTestPath) + filename;
    return chromeURL.replace(
      "chrome://mochitests/content/",
      "http://mochi.test:8888/"
    );
  }

  const url = httpURL("helper_position_sticky_flicker.html");
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  const { rect, scrollbarWidth } = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    async () => {
      const sticky = content.document.getElementById("sticky");

      // Get the area in the screen coords where the position:sticky element is.
      let stickyRect = sticky.getBoundingClientRect();
      stickyRect.x += content.window.mozInnerScreenX;
      stickyRect.y += content.window.mozInnerScreenY;

      // generate some DIVs to make the page complex enough.
      for (let i = 1; i <= 120000; i++) {
        const div = content.document.createElement("div");
        div.innerText = `${i}`;
        content.document.body.appendChild(div);
      }

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
      stickyRect.width -= w.value;
      return {
        rect: stickyRect,
        scrollbarWidth: w.value,
      };
    }
  );

  // Take a snapshot where the position:sticky element is initially painted.
  const referenceDataURL = await getSnapshot(rect);
  const referenceCanvas = await convertDataURLtoCanvas(
    referenceDataURL,
    rect.width,
    rect.height
  );

  let mouseX = window.innerWidth - scrollbarWidth / 2;
  let mouseY = tab.linkedBrowser.getBoundingClientRect().y + 5;

  // Scroll fast to cause checkerboarding multiple times.
  const dragFinisher = await promiseNativeMouseDrag(
    window,
    mouseX,
    mouseY,
    0,
    window.innerHeight,
    100
  );

  // On debug builds there seems to be no chance that the content process gets
  // painted during above promiseNativeMouseDrag call, wait two frames to make
  // sure it happens so that this test is likely able to fail without proper
  // fix.
  if (AppConstants.DEBUG) {
    await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
      await content.wrappedJSObject.promiseFrame(content.window);
      await content.wrappedJSObject.promiseFrame(content.window);
    });
  }

  // Take a snapshot again where the position:sticky element should be painted.
  const snapshotDataURL = await getSnapshot(rect);
  const snapshotCanvas = await convertDataURLtoCanvas(
    snapshotDataURL,
    rect.width,
    rect.height
  );

  await dragFinisher();

  const disablePixelAlignment = SpecialPowers.getBoolPref(
    "layout.scroll.disable-pixel-alignment"
  );
  // With disabling pixel alignment, there appears 1px line glitch at the top of
  // the image, we allow it.
  const fuzz = disablePixelAlignment
    ? { maxDifference: 1, numDifferentPixels: rect.width }
    : null;

  assertSnapshots(
    snapshotCanvas,
    referenceCanvas,
    true /* expectEqual */,
    fuzz,
    "test case",
    "reference"
  );

  BrowserTestUtils.removeTab(tab);
});
