/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/gfx/layers/apz/test/mochitest/apz_test_native_event_utils.js",
  this
);

/**
 * Test accessible is has correct offscreen state before and after
 * APZ.
 */
addAccessibleTask(
  `
  <div id="test" style="background:green; height: 100px; width: 100px;">I am square</div>
  <div style="height: 70vh;">hello world I am large</div><br>
  <h1 id="heading" style="display: inline-block;">I am a heading</h1>
  `,
  async function (browser, accDoc) {
    const test = findAccessibleChildByID(accDoc, "test");
    const heading = findAccessibleChildByID(accDoc, "heading");

    info("Verifying offscreen state");
    await untilCacheOk(() => {
      const [states] = getStates(test);
      return (states & STATE_OFFSCREEN) == 0;
    }, "test div should be ON screen");
    await untilCacheOk(() => {
      const [states] = getStates(heading);
      return (states & STATE_OFFSCREEN) == 0;
    }, "heading should be ON screen");

    info("Pinch zooming...");
    await SpecialPowers.spawn(browser, [], async () => {
      const visualScrollPromise = new Promise(resolve => {
        content.window.visualViewport.addEventListener("scroll", resolve, {
          once: true,
        });
      });
      const utils = SpecialPowers.getDOMWindowUtils(content.window);
      utils.setResolutionAndScaleTo(2);
      utils.scrollToVisual(
        200,
        200,
        utils.UPDATE_TYPE_MAIN_THREAD,
        utils.SCROLL_MODE_INSTANT
      );
      await visualScrollPromise;
    });
    info("Verifying offscreen state");
    await untilCacheOk(() => {
      const [states] = getStates(test);
      return (states & STATE_OFFSCREEN) != 0;
    }, "test div should be OFF screen");
    await untilCacheOk(() => {
      const [states] = getStates(heading);
      return (states & STATE_OFFSCREEN) != 0;
    }, "heading should be OFF screen");

    info("Scrolling to bottom of page...");
    // We have to use the APZ scroll util here, we can't rely on
    // body.scrollTo, because the thing that we've created scroll
    // bars on is the visual viewport "element".
    await SpecialPowers.spawn(browser, [], async () => {
      const visualScrollPromise = new Promise(resolve => {
        content.window.visualViewport.addEventListener("scroll", resolve, {
          once: true,
        });
      });
      const utils = SpecialPowers.getDOMWindowUtils(content.window);
      utils.scrollToVisual(
        0,
        content.visualViewport.height,
        utils.UPDATE_TYPE_MAIN_THREAD,
        utils.SCROLL_MODE_INSTANT
      );
      await visualScrollPromise;
    });

    info("Verifying offscreen state");
    await untilCacheOk(() => {
      const [states] = getStates(test);
      return (states & STATE_OFFSCREEN) != 0;
    }, "test div should be OFF screen");

    await untilCacheOk(() => {
      const [states] = getStates(heading);
      return (states & STATE_OFFSCREEN) == 0;
    }, "heading should be ON screen");
  },
  // APZ only happens on the top-level document, which means iframe tests
  // will assert differently than classic remote doc tests.
  { iframe: false, remoteIframe: false }
);
