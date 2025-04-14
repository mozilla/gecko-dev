/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test if pending requests are labeled as "" (empty string) in the Duration column.
 */

add_task(async function () {
  const { monitor } = await initNetMonitor(SLOW_REQUESTS_URL, {
    requestCount: 1,
  });
  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const { getDisplayedRequests } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );
  store.dispatch(Actions.batchEnable(false));

  let waitForPending = waitForNetworkEvents(monitor, 1, {
    expectedPayloadReady: 0,
    expectedEventTimings: 0,
  });
  info("Starting test... ");
  performRequestsInContent([
    {
      url: "sjs_long-polling-server.sjs",
      method: "GET",
    },
  ]);
  await waitForPending;
  const pendingArr = getDurations();

  waitForPending = waitForNetworkEvents(monitor, 1);
  performRequestsInContent([
    {
      url: "sjs_long-polling-server.sjs?unblock",
      method: "GET",
    },
  ]);
  await waitForPending;
  const resolvedArr = getDurations();

  is(pendingArr[0], "", "Duration should be listed as '' until resolved.");

  is(
    resolvedArr[0],
    `${getDisplayedRequests(store.getState())[0].totalTime} ms`,
    "Duration of resolved request should be displayed correctly."
  );

  function getDurations() {
    const items = document.querySelectorAll(".request-list-item");
    const result = [];

    for (const item of items) {
      const duration = item.querySelector(
        ".requests-list-duration-time"
      ).textContent;

      result.push(duration);
    }

    return result;
  }
});
