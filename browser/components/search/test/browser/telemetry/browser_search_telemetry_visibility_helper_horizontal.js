/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the visibility helper accurately calculates the horizontal
 * visibility of an element.
 */

const { VisibilityHelper } = ChromeUtils.importESModule(
  "resource:///actors/SearchSERPTelemetryChild.sys.mjs"
);

/**
 * This HTML file contains three elements in a small carousel like component.
 * The first item is far to the left to mimic an item that was scroll after
 * the user clicked on an arrow. The second item is fully visible. The third is
 * partially obscured to mimic how the a right element of a row can sometimes
 * only be partially visible unless the user clicks on an arrow and scrolls it
 * into view. The fourth element is to mimic an element that's not visible.
 */
const TEST_URI = `
<!DOCTYPE html>
<main>
  <style>
    html {
      margin: 0;
    }
    .flex-container {
      display: flex;
      gap: 10px;
    }
    .flex-container > div {
      background: black;
      height: 200px;
      width: 300px;
    }
  </style>
  <section id="carousel" style="width: 500px; height: 200px; overflow: hidden; position: relative;">
    <div style="position: absolute; left: -300px;" class="flex-container">
      <div id="item-1">Item 1</div>
      <div id="item-2">Item 2</div>
      <div id="item-3">Item 3</div>
      <div id="item-4">Item 4</div>
    </div>
  </section>
</main>
`;
const URL =
  "https://example.com/document-builder.sjs?html=" +
  encodeURIComponent(TEST_URI);

// How much of the element width should be shown.
const STRICT_THRESHOLD = 1.0;
const LOOSE_THRESHOLD = 0.1;

const WINDOW_HEIGHT = 768;
const WINDOW_WIDTH = 1024;

async function checkHorizontalVisibility(tab, parentId, childId, threshold) {
  let parentRect = await extractVisibilityDOMRect(tab, parentId);
  let childRect = await extractVisibilityDOMRect(tab, childId);
  let visible = VisibilityHelper.childElementWasVisibleHorizontally(
    parentRect,
    childRect,
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

add_setup(async function () {
  let originalWidth = window.outerWidth;
  let originalHeight = window.outerHeight;
  await resizeWindow(window, WINDOW_WIDTH, WINDOW_HEIGHT);
  registerCleanupFunction(async () => {
    await resizeWindow(window, originalWidth, originalHeight);
    resetTelemetry();
  });
});

add_task(async function test_horizontal_strict_threshold() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);

  info("Checking hidden element to the left.");
  let visible = await checkHorizontalVisibility(
    tab,
    "carousel",
    "item-1",
    STRICT_THRESHOLD
  );
  Assert.equal(visible, true, "Should be considered visible.");

  info("Checking element in the center.");
  visible = await checkHorizontalVisibility(
    tab,
    "carousel",
    "item-2",
    STRICT_THRESHOLD
  );
  Assert.equal(visible, true, "Should be considered visible.");

  info("Checking partially viewable element on the right.");
  visible = await checkHorizontalVisibility(
    tab,
    "carousel",
    "item-3",
    STRICT_THRESHOLD
  );
  Assert.equal(visible, false, "Should not be considered visible.");

  info("Checking non viewable element on the right.");
  visible = await checkHorizontalVisibility(
    tab,
    "carousel",
    "item-4",
    STRICT_THRESHOLD
  );
  Assert.equal(visible, false, "Should not be considered visible.");

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_horizontal_loose_threshold() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);

  info("Checking hidden element to the left.");
  let visible = await checkHorizontalVisibility(
    tab,
    "carousel",
    "item-1",
    LOOSE_THRESHOLD
  );
  Assert.equal(visible, true, "Should be considered visible.");

  info("Checking element in the center.");
  visible = await checkHorizontalVisibility(
    tab,
    "carousel",
    "item-2",
    LOOSE_THRESHOLD
  );
  Assert.equal(visible, true, "Should be considered visible.");

  info("Checking partially viewable to the right.");
  visible = await checkHorizontalVisibility(
    tab,
    "carousel",
    "item-3",
    LOOSE_THRESHOLD
  );
  Assert.equal(visible, true, "Should be considered visible.");

  info("Checking non viewable element on the right.");
  visible = await checkHorizontalVisibility(
    tab,
    "carousel",
    "item-4",
    LOOSE_THRESHOLD
  );
  Assert.equal(visible, false, "Should not be considered visible.");

  await BrowserTestUtils.removeTab(tab);
});
