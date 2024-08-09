/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/*
 * This test duplicates image/test/mochitest/test_animated_css_image.html, so keep them in sync.
 * This is because we need a browser-chrome test in order to test invalidation (the getSnapshot method here
 * uses the same path as painting to the screen, whereas test_animated_css_image.html is doing a
 * separate paint to a surface), but browser-chrome isn't run on android, so test_animated_css_image.html
 * gets us android coverage.
 */

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

// this contains the kTests array
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/image/test/browser/animated_image_test_list.js",
  this
);

async function assertAnimates(thehtml) {
  function httpURL(sfilename) {
    let chromeURL = getRootDirectory(gTestPath) + sfilename;
    return chromeURL.replace(
      "chrome://mochitests/content/",
      "http://mochi.test:8888/"
    );
  }

  const url = httpURL("helper_animated_css_image.html");
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  const { rect } = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    async () => {
      let rect = content.document.documentElement.getBoundingClientRect();
      rect.x += content.window.mozInnerScreenX;
      rect.y += content.window.mozInnerScreenY;

      return {
        rect,
      };
    }
  );

  let blankSnapshot = await getSnapshot({
    x: rect.x,
    y: rect.y,
    width: rect.width,
    height: rect.height,
  });

  const kNumRetries = 600;

  info("testing: " + thehtml);

  await SpecialPowers.spawn(tab.linkedBrowser, [thehtml], async thehtml => {
    const theiframe = content.document.getElementById("iframe");
    let load = new Promise(resolve => {
      theiframe.addEventListener("load", resolve, { once: true });
    });
    theiframe.srcdoc = thehtml;
    await load;

    // give time for content/test load handlers to run before we do anything
    await new Promise(resolve => {
      content.window.requestAnimationFrame(() => {
        content.window.requestAnimationFrame(resolve);
      });
    });

    // make sure we are flushed and rendered.
    content.document.documentElement.getBoundingClientRect();

    await new Promise(resolve => {
      content.window.requestAnimationFrame(() => {
        content.window.requestAnimationFrame(resolve);
      });
    });
  });

  let initial = await getSnapshot({
    x: rect.x,
    y: rect.y,
    width: rect.width,
    height: rect.height,
  });

  {
    // One test (bug 1730834) loads an image as the background of a div in the
    // load handler, so there's no good way to wait for it to be loaded and
    // rendered except to poll.
    let equal = initial == blankSnapshot;
    for (let i = 0; i < kNumRetries; ++i) {
      if (!equal) {
        break;
      }

      await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
        await new Promise(resolve => {
          content.window.requestAnimationFrame(() => {
            content.window.requestAnimationFrame(resolve);
          });
        });
      });

      initial = await getSnapshot({
        x: rect.x,
        y: rect.y,
        width: rect.width,
        height: rect.height,
      });
      equal = initial == blankSnapshot;
    }
    ok(!equal, "Initial snapshot shouldn't be blank");
  }

  async function checkFrames() {
    let foundDifferent = false;
    let foundInitialAgain = false;
    for (let i = 0; i < kNumRetries; ++i) {
      let current = await getSnapshot({
        x: rect.x,
        y: rect.y,
        width: rect.width,
        height: rect.height,
      });

      let equal = initial == current;
      if (!foundDifferent && !equal) {
        ok(true, `Found different image after ${i} retries`);
        foundDifferent = true;
      }

      // Ensure that we go back to the initial state (animated1.gif) is an
      // infinite gif.
      if (foundDifferent && equal) {
        ok(true, `Found same image again after ${i} retries`);

        foundInitialAgain = true;
        break;
      }

      await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
        await new Promise(resolve => {
          content.window.requestAnimationFrame(() => {
            content.window.requestAnimationFrame(resolve);
          });
        });
      });
    }

    ok(
      foundDifferent && foundInitialAgain,
      `Should've found a different snapshot and then an equal one, after ${kNumRetries} retries`
    );
  }

  for (let j = 0; j < 5; j++) {
    await checkFrames();
  }

  BrowserTestUtils.removeTab(tab);
}

add_task(async () => {
  // kTests is defined in the imported animated_image_test_list.js so it can
  // be shared between tests.
  // eslint-disable-next-line no-undef
  for (let { html } of kTests) {
    await assertAnimates(html);
  }
});
