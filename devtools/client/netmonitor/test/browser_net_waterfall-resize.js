/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that waterfall timing elements are resized when the column is resized.
 */

add_task(async function () {
  // Use a test page with relatively slow requests.
  const { tab, monitor } = await initNetMonitor(
    CONTENT_TYPE_WITHOUT_CACHE_URL,
    { requestCount: 1 }
  );
  const { document, store } = monitor.panelWin;

  // Execute requests.
  await performRequests(monitor, tab, CONTENT_TYPE_WITHOUT_CACHE_REQUESTS);

  info("Mesure the initial width of a request timing element");
  const initialWidth = await getStableTimingBlockWidth(document);

  const headers = document.querySelector(".requests-list-headers");
  const parentWidth = headers.getBoundingClientRect().width;

  info("Resize waterfall column");
  const waterfallHeader = document.querySelector(
    `#requests-list-waterfall-header-box`
  );

  const onWaterfallResize = waitForDispatch(store, "WATERFALL_RESIZE");
  resizeWaterfallColumn(waterfallHeader, 30, parentWidth);
  await onWaterfallResize;

  info("Mesure the request timing element again");
  const finalWidth = await getStableTimingBlockWidth(document);

  // We want to check that finalWidth is different from initialWidth, but
  // the size might be slightly updated so we can't use isnot to assert this.
  // The finalWidth should be significantly bigger, so check the difference is
  // at least greater than 1.
  Assert.greater(
    Math.abs(finalWidth - initialWidth),
    1,
    "The request timing element should be updated"
  );
  return teardown(monitor);
});

/**
 * Timings are updated with a slight delay so wait until the dimension is
 * stabilized.
 */
async function getStableTimingBlockWidth(doc) {
  let stableWidth = -1;

  // Use a bigger interval on slow platforms.
  const interval =
    AppConstants.ASAN || AppConstants.DEBUG || AppConstants.TSAN ? 1000 : 500;

  await waitFor(
    () => {
      const firstBlock = doc.querySelector(".requests-list-timings-box");
      const width = firstBlock.getBoundingClientRect().width;
      if (width == stableWidth) {
        return true;
      }
      stableWidth = width;
      return false;
    },
    "Wait for the element width to stabilize",
    interval
  );

  return stableWidth;
}
