/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if resending a CORS request avoids the security checks and doesn't send
 * a preflight OPTIONS request (bug 1270096 and friends)
 */

add_task(async function () {
  const { tab, monitor } = await initNetMonitor(HTTPS_CORS_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { store, windowRequire, connector } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const { getRequestById, getSortedRequests } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );

  store.dispatch(Actions.batchEnable(false));

  const requestUrl = "https://test1.example.com" + CORS_SJS_PATH;

  info("Waiting for OPTIONS, then POST");
  const onEvents = waitForNetworkEvents(monitor, 2);
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [requestUrl],
    async function (url) {
      content.wrappedJSObject.performRequests(
        url,
        "triggering/preflight",
        "post-data"
      );
    }
  );
  await onEvents;

  // Check the requests that were sent
  let sortedRequests = getSortedRequests(store.getState());

  // If the NetworkObserver is configured to use early events, we should detect
  // the flight request (POST) before the preflight request (OPTIONS).
  // Bug 1901504 will remove the earlyEvents=false option.
  // Note that this test is still intermittently detecting the preflight request
  // before the flight request, event with early events enabled, so we should
  // keep the logic here to assign optRequest and postRequest accordingly.
  let optRequest, postRequest;
  if (sortedRequests[0].method === "POST") {
    optRequest = sortedRequests[1];
    postRequest = sortedRequests[0];
  } else {
    optRequest = sortedRequests[0];
    postRequest = sortedRequests[1];
  }

  is(optRequest.method, "OPTIONS", `The OPTIONS request has the right method`);
  is(optRequest.url, requestUrl, `The OPTIONS request has the right URL`);

  is(postRequest.method, "POST", `The POST request has the right method`);
  is(postRequest.url, requestUrl, `The POST request has the right URL`);

  // Resend both requests without modification. Wait for resent OPTIONS, then POST.
  // POST is supposed to have no preflight OPTIONS request this time (CORS is disabled)
  for (let item of [optRequest, postRequest]) {
    const onRequest = waitForNetworkEvents(monitor, 1);
    info(`Selecting the ${item.method} request`);
    store.dispatch(Actions.selectRequest(item.id));

    // Wait for requestHeaders and responseHeaders are required when fetching data
    // from back-end.
    await waitUntil(() => {
      item = getRequestById(store.getState(), item.id);
      return item.requestHeaders && item.responseHeaders;
    });

    info(`Cloning the ${item.method} request into a custom clone`);
    store.dispatch(Actions.cloneRequest(item.id));

    info("Sending the cloned request (without change)");
    store.dispatch(Actions.sendCustomRequest(item.id));

    info("Waiting for the resent request");
    await onRequest;
  }

  // Retrieve the new list of sorted requests, which should include 2 resent
  // requests.
  sortedRequests = getSortedRequests(store.getState());
  is(sortedRequests.length, 4, "There are 4 requests in total");

  // If the NetworkObserver is configured to use early events, we should detect
  // the flight request (POST) before the preflight request (OPTIONS).
  // Bug 1901504 will remove the earlyEvents=false option.
  // Note that this test is still intermittently detecting the preflight request
  // before the flight request, event with early events enabled, so we should
  // keep the logic here to assign resentOptRequest and resentPostRequest
  // accordingly.
  let resentOptRequest, resentPostRequest;
  if (sortedRequests[2].method === "POST") {
    resentOptRequest = sortedRequests[3];
    resentPostRequest = sortedRequests[2];
  } else {
    resentOptRequest = sortedRequests[2];
    resentPostRequest = sortedRequests[3];
  }
  is(
    resentOptRequest.method,
    "OPTIONS",
    `The resent OPTIONS request has the right method`
  );
  is(
    resentOptRequest.url,
    requestUrl,
    `The resent OPTIONS request has the right URL`
  );
  is(
    resentOptRequest.status,
    "200",
    `The resent OPTIONS response has the right status`
  );
  is(
    resentOptRequest.blockedReason,
    0,
    `The resent OPTIONS request was not blocked`
  );

  is(
    resentPostRequest.method,
    "POST",
    `The resent POST request has the right method`
  );
  is(
    resentPostRequest.url,
    requestUrl,
    `The resent POST request has the right URL`
  );
  is(
    resentPostRequest.status,
    "200",
    `The resent POST response has the right status`
  );
  is(
    resentPostRequest.blockedReason,
    0,
    `The resent POST request was not blocked`
  );

  await Promise.all([
    connector.requestData(resentPostRequest.id, "requestPostData"),
    connector.requestData(resentPostRequest.id, "responseContent"),
  ]);

  // Wait until responseContent and requestPostData are available.
  await waitUntil(() => {
    resentPostRequest = getRequestById(store.getState(), resentPostRequest.id);
    return (
      resentPostRequest.responseContent && resentPostRequest.requestPostData
    );
  });

  is(
    resentPostRequest.requestPostData.postData.text,
    "post-data",
    "The resent POST request has the right POST data"
  );
  is(
    resentPostRequest.responseContent.content.text,
    "Access-Control-Allow-Origin: *",
    "The resent POST response has the right content"
  );

  info("Finishing the test");
  return teardown(monitor);
});
