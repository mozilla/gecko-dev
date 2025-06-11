/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that creating a thumbnail of an svg document works.

function getSVGUrl(fill) {
  return (
    `data:image/svg+xml,` +
    encodeURIComponent(`<svg width="300" height="300" xmlns="http://www.w3.org/2000/svg">
      <rect width="300" height="300" x="0" y="0" fill="${fill}" />
    </svg>`)
  );
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["test.wait300msAfterTabSwitch", true]],
  });
});

add_task(async function test_capture_svg() {
  // Create a tab with a red background.
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: getSVGUrl("red"),
    },
    async browser => {
      await captureAndCheckColor(255, 0, 0, "we have a red thumbnail");

      // Load a page with a green background.
      let loaded = BrowserTestUtils.browserLoaded(browser);
      BrowserTestUtils.startLoadingURIString(browser, getSVGUrl("lime"));
      await loaded;
      await captureAndCheckColor(0, 255, 0, "we have a green thumbnail");

      // Load a page with a blue background.
      loaded = BrowserTestUtils.browserLoaded(browser);
      BrowserTestUtils.startLoadingURIString(browser, getSVGUrl("blue"));
      await loaded;
      await captureAndCheckColor(0, 0, 255, "we have a blue thumbnail");
    }
  );
});
