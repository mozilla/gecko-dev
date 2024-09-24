/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the script caches in the network monitor.
add_task(async function () {
  const { monitor } = await initNetMonitor(SCRIPT_CACHE_URL, {
    enableCache: true,
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const { getDisplayedRequests, getSortedRequests } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  const waitForEvents = waitForNetworkEvents(monitor, 4);

  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    SCRIPT_CACHE_URL
  );
  await waitForEvents;

  const requests = document.querySelectorAll(".request-list-item");

  is(requests.length, 4, "There should be 4 requests");

  const requestData = {
    uri: HTTPS_EXAMPLE_URL + "sjs_test-script.sjs",
    details: {
      status: 200,
      statusText: "OK (cached)",
      displayedStatus: "cached",
      type: "js",
      fullMimeType: "text/javascript",
    },
  };

  // All script elements should create corresponding requests.
  for (let i = 1; i < requests.length; ++i) {
    const request = requests[i];

    const requestsListStatus = request.querySelector(".status-code");
    EventUtils.sendMouseEvent({ type: "mouseover" }, requestsListStatus);
    await waitUntil(() => requestsListStatus.title);

    await verifyRequestItemTarget(
      document,
      getDisplayedRequests(store.getState()),
      getSortedRequests(store.getState())[i],
      "GET",
      requestData.uri,
      requestData.details
    );
  }
  await teardown(monitor);
});
