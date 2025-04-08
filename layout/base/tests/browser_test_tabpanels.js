/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This test is based on
     https://searchfox.org/mozilla-central/rev/380b8fd795e7d96d8a5a3e6ec2b50a9f2b65854a/layout/base/tests/browser_test_oopif_reconstruct.js
*/

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/gfx/layers/apz/test/mochitest/apz_test_utils.js",
  this
);

async function runOneTest(filename) {
  function chromeURL(sfilename) {
    let result = getRootDirectory(gTestPath) + sfilename;
    return result;
  }

  const url = chromeURL(filename);
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  const { rect } = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    async () => {
      const container = content.document.documentElement;

      // Get the area in the screen coords of the tab.
      let containerRect = container.getBoundingClientRect();
      containerRect.x += content.window.mozInnerScreenX;
      containerRect.y += content.window.mozInnerScreenY;

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

  BrowserTestUtils.removeTab(tab);
  return reference;
}

add_task(async () => {
  let snapshot1 = await runOneTest("470711-1.xhtml");
  let snapshot2 = await runOneTest("470711-1-ref.xhtml");
  is(snapshot1, snapshot1, "should be same");
});
