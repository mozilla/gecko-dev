/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that a toggle hidden behind a semi-transparent overlay that causes the
 * toggle to be under the visibility threshold, does not cause PiP to open if
 * the overlay happens to have will-change set on it. (bug 1936819)
 */
add_task(async function test_transparent_will_change() {
  Services.ppmm.sharedData.set(SHARED_DATA_KEY, {
    "*://example.com/*": { visibilityThreshold: 0.5 },
  });
  Services.ppmm.sharedData.flush();

  const PAGE = TEST_ROOT + "test-transparent-overlay-3.html";
  await testToggle(PAGE, {
    "video-partial-opacity": { canToggle: false },
  });

  Services.ppmm.sharedData.set(SHARED_DATA_KEY, {});
  Services.ppmm.sharedData.flush();
});
