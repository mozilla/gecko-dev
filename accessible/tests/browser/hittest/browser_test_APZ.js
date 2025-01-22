/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/gfx/layers/apz/test/mochitest/apz_test_native_event_utils.js",
  this
);

/**
 * Test accessible is hittestable before and after APZ. Also verify
 * bounds change in the direction we expect.
 *
 * Note: the `display:inline-block` styling ensures elements that would
 * otherwise span the entire page in width do not extend beyond their
 * visual boundaries. Without it, we'd still "hit" items in the reduced
 * visual viewport even though their content doesn't appear on screen.
 */
addAccessibleTask(
  `
  <div id="test" role="button" style="background:green; min-height: 10vh; max-width: 10vh; display:inline-block;">I am square</div><br>
  <div style="height: 70vh;">hello world I am large</div><br>
  <h1 id="heading" style="display:inline-block;">I am a heading</h1>
  `,
  async function (browser, accDoc) {
    const test = findAccessibleChildByID(accDoc, "test");

    info("Hittesting pre-APZ");
    let dpr = await getContentDPR(browser);
    let [targetX, targetY, targetW, targetH] = Layout.getBounds(test, dpr);
    let [x, y] = Layout.getBounds(accDoc, dpr);
    await testChildAtPoint(
      dpr,
      targetX - x + targetW / 2,
      targetY - y + targetH / 2,
      accDoc,
      test, // Direct Child
      test // Deepest Child
    );

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
    info("Hittesting post-APZ");
    dpr = await getContentDPR(browser);
    let [newX, newY, newW, newH] = Layout.getBounds(test, dpr);
    [x, y] = Layout.getBounds(accDoc, dpr);
    // This shouldn't be in the viewport cache, because it is out of the
    // visual viewport
    await testChildAtPoint(
      dpr,
      newX - x + newW / 2,
      newY - y + newH / 2,
      accDoc,
      null, // Direct Child
      null // Deepest Child
    );

    // We pinch zoom to the right of the square,
    // which means its coords become off-screen
    // (negative).
    info("Verifying scaled bounds");
    Assert.less(newX, 0, "X coord should be smaller than 0");
    Assert.less(newY, 0, "Y coord should be smaller than 0");
    // Because we zoomed in, width and height should
    // be larger than they were before.
    Assert.greater(newW, targetW, "Width should be larger than old width");
    Assert.greater(newH, targetH, "Height should be larger than old height");
  },
  // APZ only happens on the top-level document, which means iframe tests
  // will assert differently than classic remote doc tests.
  { iframe: false, remoteIframe: false }
);

/**
 * Test accessible is hittestable before and after APZ with scroll. Also verify
 * the viewport cache is cleared of "old" information.
 */
addAccessibleTask(
  `
  <div id="test" role="button" style="background:green; min-height: 10vh; max-width: 10vh; display:inline-block;">I am square</div><br>
  <div id="spacer" style="min-height: 70vh; display:inline-block;">hello world I am large</div><br>
  <h1 id="heading" style="display:inline-block;">I am a heading</h1>
  `,
  async function (browser, accDoc) {
    const test = findAccessibleChildByID(accDoc, "test");
    const heading = findAccessibleChildByID(accDoc, "heading");

    info("Hittesting pre-APZ");
    let dpr = await getContentDPR(browser);
    let [targetX, targetY, targetW, targetH] = Layout.getBounds(test, dpr);
    let [x, y] = Layout.getBounds(accDoc, dpr);
    // Save these values for later, we'll need them to compare.
    const origTestX = targetX - x + targetW / 2;
    const origTestY = targetY - y + targetH / 2;
    // Try hittesting the test node, which should succeed.
    await testChildAtPoint(
      dpr,
      origTestX,
      origTestY,
      accDoc,
      test, // Direct Child
      test // Deepest Child
    );

    // Now try hittesting the heading, we should get the heading and its internal
    // text node.
    [targetX, targetY, targetW, targetH] = Layout.getBounds(heading, dpr);
    const origHeadingX = targetX - x + targetW / 2;
    const origHeadingY = targetY - y + targetH / 2;
    await testChildAtPoint(
      dpr,
      origHeadingX,
      origHeadingY,
      accDoc,
      heading, // Direct Child
      heading.firstChild // Deepest Child
    );

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

    info("Hittesting post-APZ, pre-scroll");
    dpr = await getContentDPR(browser);
    let [newX, newY, newW, newH] = Layout.getBounds(heading, dpr);
    [x, y] = Layout.getBounds(accDoc, dpr);
    // Try hittesting the heading, which should be outside
    // the visual viewport.
    info("Testing heading at new point");
    await testChildAtPoint(
      dpr,
      newX - x + newW / 2,
      newY - y + newH / 2,
      accDoc,
      null, // Direct Child
      null // Deepest Child
    );

    info("Testing heading at old point");
    // To ensure the viewport cache has updated, and that the heading isn't
    // stuck in its old location, test there too. If this fails, we know we're
    // missing a viewport update.
    await testChildAtPoint(
      dpr,
      origHeadingX,
      origHeadingY,
      accDoc,
      accDoc, // Direct Child
      accDoc // Deepest Child
    );

    // Then try hittesting the original div, which should
    // also be outside the visual viewport
    [newX, newY, newW, newH] = Layout.getBounds(test, dpr);
    info("testing test at new point");
    await testChildAtPoint(
      dpr,
      newX - x + newW / 2,
      newY - y + newH / 2,
      accDoc,
      null, // Direct Child
      null // Deepest Child
    );
    info("testing test at old point");
    // If we still get the test div, the viewport cache is
    // stale.
    await testChildAtPoint(
      dpr,
      origTestX,
      origTestY,
      accDoc,
      accDoc, // Direct Child
      accDoc // Deepest Child
    );

    info("Scrolling to bottom of page...");
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

    info("Hittesting post-APZ, post-scroll");
    dpr = await getContentDPR(browser);
    [newX, newY, newW, newH] = Layout.getBounds(test, dpr);
    [x, y] = Layout.getBounds(accDoc, dpr);
    // We shouldn't get anything in the spot that the test acc
    // previously occupied, it should be offscreen.
    info("testing test at new point");
    await testChildAtPoint(
      dpr,
      newX - x + newW / 2,
      newY - y + newH / 2,
      accDoc,
      null, // Direct Child
      null // Deepest Child
    );
    info("Testing test at old point");
    const spacer = findAccessibleChildByID(accDoc, "spacer");
    // "Spacer" takes up all the space above the heading, which means
    // it occupies the region our test div originally did. We
    // should find it here, if the viewport cache isn't stale.
    await testChildAtPoint(
      dpr,
      origTestX,
      origTestY,
      accDoc,
      spacer, // Direct Child
      spacer // Deepest Child
    );

    // We should be able to hittest the heading at the
    // bottom of the page.
    [newX, newY, newW, newH] = Layout.getBounds(heading, dpr);
    info("Testing heading at new point");
    await testChildAtPoint(
      dpr,
      newX - x + newW / 2,
      newY - y + newH / 2,
      accDoc,
      heading, // Direct Child
      heading.firstChild // Deepest Child
    );
  },
  // APZ only happens on the top-level document, which means iframe tests
  // will assert differently than classic remote doc tests.
  { iframe: false, remoteIframe: false }
);
