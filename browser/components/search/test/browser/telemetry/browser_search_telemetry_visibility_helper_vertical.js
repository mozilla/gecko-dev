/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the visibility helper accurately calculates the vertical
 * visibility of an element.
 */

const { VisibilityHelper } = ChromeUtils.importESModule(
  "resource:///actors/SearchSERPTelemetryChild.sys.mjs"
);

/**
 * This HTML file contains three elements: One above the fold, one that is
 * partially at the fold, and one that is below the fold. The tests then check
 * if they are considered visible depending on how much the page has been
 * scrolled and the threshold.
 */
const TEST_URI = `
<!DOCTYPE html>
<main>
  <style>
    html {
      margin: 0;
    }
    div {
      background: black;
      height: 300px;
      width: 400px;
      color: #FFF;
      padding: 10px;
    }
  </style>
  <div id="above-fold" style="margin-bottom: 200px;">Top</div>
  <div id="at-fold" style="margin-bottom: 50px;">Middle</div>
  <div id="below-fold" style="margin-bottom: 50px;">Bottom</div>
</main>
`;
const URL =
  "https://example.com/document-builder.sjs?html=" +
  encodeURIComponent(TEST_URI);

// How much of the element height or width should be shown.
const STRICT_THRESHOLD = 1.0;
const LOOSE_THRESHOLD = 0.1;

// Roughly enough to show the second element and part of the third element.
const SCROLL_BELOW_FOLD = 400;

const WINDOW_HEIGHT = 768;
const WINDOW_WIDTH = 1024;

async function checkVerticalVisibility(tab, id, threshold) {
  let domRect = await extractVisibilityDOMRect(tab, id);
  let visible = VisibilityHelper.elementWasVisibleVertically(
    domRect,
    tab.linkedBrowser.clientHeight,
    threshold
  );
  return visible;
}

async function extractVisibilityDOMRect(tab, id) {
  let domRect = await SpecialPowers.spawn(tab.linkedBrowser, [id], _id => {
    return content.document.getElementById(_id).getBoundingClientRect();
  });
  return domRect;
}

async function waitForScrollEvent(aBrowser, aTask) {
  let promise = BrowserTestUtils.waitForContentEvent(aBrowser, "scroll");

  // This forces us to send a message to the browser's process and receive a response which ensures
  // that the message sent to register the scroll event listener will also have been processed by
  // the content process. Without this it is possible for our scroll task to send a higher priority
  // message which can be processed by the content process before the message to register the scroll
  // event listener.
  await SpecialPowers.spawn(aBrowser, [], () => {});

  await aTask();
  await promise;
}

add_setup(async function () {
  let originalWidth = window.outerWidth;
  let originalHeight = window.outerHeight;
  await resizeWindow(window, WINDOW_WIDTH, WINDOW_HEIGHT);
  registerCleanupFunction(async () => {
    await resizeWindow(window, originalWidth, originalHeight);
    resetTelemetry();
  });
});

add_task(async function test_vertical_visibility_no_scroll_strict_threshold() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);

  info("Element above the fold.");
  let visible = await checkVerticalVisibility(
    tab,
    "above-fold",
    STRICT_THRESHOLD
  );
  Assert.equal(visible, true, "Should be considered visible.");

  info("Element at the fold.");
  visible = await checkVerticalVisibility(tab, "at-fold", STRICT_THRESHOLD);
  Assert.equal(visible, false, "Should not be considered visible.");

  info("Element below the fold.");
  visible = await checkVerticalVisibility(tab, "below-fold", STRICT_THRESHOLD);
  Assert.equal(visible, false, "Should not be considered visible.");

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_vertical_visibility_no_scroll_loose_threshold() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);

  info("Element above the fold.");
  let visible = await checkVerticalVisibility(
    tab,
    "above-fold",
    LOOSE_THRESHOLD
  );
  Assert.equal(visible, true, "Should be considered visible.");

  info("Element at the fold.");
  visible = await checkVerticalVisibility(tab, "at-fold", LOOSE_THRESHOLD);
  Assert.equal(visible, true, "Should be considered visible.");

  info("Element below the fold.");
  visible = await checkVerticalVisibility(tab, "below-fold", LOOSE_THRESHOLD);
  Assert.equal(visible, false, "Should not considered visible.");

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_vertical_visibility_scroll_strict_threshold() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);

  info("Scroll near the bottom but don't completely show the last element.");
  await waitForScrollEvent(tab.linkedBrowser, () =>
    SpecialPowers.spawn(tab.linkedBrowser, [SCROLL_BELOW_FOLD], y => {
      content.scrollBy(0, y);
    })
  );

  info("Element above the fold.");
  let visible = await checkVerticalVisibility(
    tab,
    "above-fold",
    STRICT_THRESHOLD
  );
  Assert.equal(visible, true, "Should be considered visible.");

  info("Element at the fold that is now fully visible.");
  visible = await checkVerticalVisibility(tab, "at-fold", STRICT_THRESHOLD);
  Assert.equal(visible, true, "Should be considered visible.");

  info("Element below the fold that is still not fully visible.");
  visible = await checkVerticalVisibility(tab, "below-fold", STRICT_THRESHOLD);
  Assert.equal(visible, false, "Should not be considered visible.");

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_vertical_visibility_scroll_loose_threshold() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);

  info("Scroll near the bottom but don't completely show the last element.");
  await waitForScrollEvent(tab.linkedBrowser, () =>
    SpecialPowers.spawn(tab.linkedBrowser, [SCROLL_BELOW_FOLD], y => {
      content.scrollBy(0, y);
    })
  );

  info("Element above the fold.");
  let visible = await checkVerticalVisibility(
    tab,
    "above-fold",
    LOOSE_THRESHOLD
  );
  Assert.equal(visible, true, "Should be considered visible.");

  info("Element at the fold that is now fully visible.");
  visible = await checkVerticalVisibility(tab, "at-fold", LOOSE_THRESHOLD);
  Assert.equal(visible, true, "Should be considered visible.");

  info("Element below the fold that is still not fully visible.");
  visible = await checkVerticalVisibility(tab, "below-fold", LOOSE_THRESHOLD);
  Assert.equal(visible, true, "Should be considered visible.");

  await BrowserTestUtils.removeTab(tab);
});
