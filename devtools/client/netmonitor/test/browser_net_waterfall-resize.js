/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that waterfall timing elements are resized when the column is resized.
 */

add_task(async function () {
  // Use a test page with slow requests.
  const { monitor } = await initNetMonitor(HTTPS_SLOW_REQUESTS_URL, {
    requestCount: 2,
  });
  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  const onEvents = waitForNetworkEvents(monitor, 2);
  await reloadBrowser();
  await onEvents;

  info("Resize waterfall column a first time");
  const waterfallHeader = document.querySelector(
    `#requests-list-waterfall-header-box`
  );
  const headers = document.querySelector(".requests-list-headers");
  const parentWidth = headers.getBoundingClientRect().width;

  let onWaterfallResize = waitForDispatch(store, "WATERFALL_RESIZE");
  resizeWaterfallColumn(waterfallHeader, 30, parentWidth);
  await onWaterfallResize;

  info("Mesure the initial width of a request timing element");
  const initialWidth = await getStableTimingBoxesWidth(document);
  info("Measured initialWidth: " + initialWidth);

  info("Resize waterfall column again");
  onWaterfallResize = waitForDispatch(store, "WATERFALL_RESIZE");
  resizeWaterfallColumn(waterfallHeader, 60, parentWidth);
  await onWaterfallResize;

  info("Mesure the request timing element again");
  const finalWidth = await getStableTimingBoxesWidth(
    document,
    width => width > 200
  );
  info("Measured finalWidth: " + finalWidth);

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
 * Measure the widths of all waterfall timing boxes.
 *
 * @param {Document} doc
 *     Netmonitor document.
 * @param {function=} predicate
 *     Optional predicate to avoid returning erroneous width. On windows CI,
 *     the second measure is hard to get right, so we use the predicate to make
 *     sure we retrieve the good size.
 * @returns {number}
 *     The measured width.
 */
async function getStableTimingBoxesWidth(doc, predicate = null) {
  let stableWidth = -1;
  await waitFor(
    () => {
      // Sum the width of all displayed timings.
      const timingBoxes = [
        ...doc.querySelectorAll(".requests-list-timings-box"),
      ];
      const widths = timingBoxes.map(el => el.getBoundingClientRect().width);
      const width = widths.reduce((sum, w) => sum + w, 0);

      // If the width changed, updated it and return.
      if (width != stableWidth) {
        stableWidth = width;
        return false;
      }

      // Otherwise, check the predicate if provided.
      if (typeof predicate == "function") {
        return predicate(width);
      }

      return true;
    },
    "Wait for the timings width to stabilize",
    500
  );

  return stableWidth;
}
