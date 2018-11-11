/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if request cause is reported correctly.
 */

const CAUSE_FILE_NAME = "html_cause-test-page.html";
const CAUSE_URL = EXAMPLE_URL + CAUSE_FILE_NAME;

const EXPECTED_REQUESTS = [
  {
    method: "GET",
    url: CAUSE_URL,
    causeType: "document",
    causeUri: null,
    // The document load has internal privileged JS code on the stack
    stack: true,
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "stylesheet_request",
    causeType: "stylesheet",
    causeUri: CAUSE_URL,
    stack: false,
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "img_request",
    causeType: "img",
    causeUri: CAUSE_URL,
    stack: false,
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "xhr_request",
    causeType: "xhr",
    causeUri: CAUSE_URL,
    stack: [{ fn: "performXhrRequestCallback", file: CAUSE_FILE_NAME, line: 26 }],
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "fetch_request",
    causeType: "fetch",
    causeUri: CAUSE_URL,
    stack: [{ fn: "performFetchRequest", file: CAUSE_FILE_NAME, line: 31 }],
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "promise_fetch_request",
    causeType: "fetch",
    causeUri: CAUSE_URL,
    stack: [
      { fn: "performPromiseFetchRequestCallback", file: CAUSE_FILE_NAME, line: 37 },
      { fn: "performPromiseFetchRequest", file: CAUSE_FILE_NAME, line: 36,
        asyncCause: "promise callback" },
    ],
  },
  {
    method: "GET",
    url: EXAMPLE_URL + "timeout_fetch_request",
    causeType: "fetch",
    causeUri: CAUSE_URL,
    stack: [
      { fn: "performTimeoutFetchRequestCallback2", file: CAUSE_FILE_NAME, line: 44 },
      { fn: "performTimeoutFetchRequestCallback1", file: CAUSE_FILE_NAME, line: 43,
        asyncCause: "setTimeout handler" },
    ],
  },
  {
    method: "POST",
    url: EXAMPLE_URL + "beacon_request",
    causeType: "beacon",
    causeUri: CAUSE_URL,
    stack: [{ fn: "performBeaconRequest", file: CAUSE_FILE_NAME, line: 50 }],
  },
];

add_task(async function() {
  // Async stacks aren't on by default in all builds
  await SpecialPowers.pushPrefEnv({ set: [["javascript.options.asyncstack", true]] });

  // the initNetMonitor function clears the network request list after the
  // page is loaded. That's why we first load a bogus page from SIMPLE_URL,
  // and only then load the real thing from CAUSE_URL - we want to catch
  // all the requests the page is making, not only the XHRs.
  // We can't use about:blank here, because initNetMonitor checks that the
  // page has actually made at least one request.
  const { tab, monitor } = await initNetMonitor(SIMPLE_URL);

  const { document, store, windowRequire, connector } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const {
    getDisplayedRequests,
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  const wait = waitForNetworkEvents(monitor, EXPECTED_REQUESTS.length);
  BrowserTestUtils.loadURI(tab.linkedBrowser, CAUSE_URL);
  await wait;

  const requests = getSortedRequests(store.getState());
  await Promise.all(requests.map(requestItem =>
    connector.requestData(requestItem.id, "stackTrace")));

  is(store.getState().requests.requests.size, EXPECTED_REQUESTS.length,
    "All the page events should be recorded.");

  EXPECTED_REQUESTS.forEach((spec, i) => {
    const { method, url, causeType, causeUri, stack } = spec;

    const requestItem = getSortedRequests(store.getState()).get(i);
    verifyRequestItemTarget(
      document,
      getDisplayedRequests(store.getState()),
      requestItem,
      method,
      url,
      { cause: { type: causeType, loadingDocumentUri: causeUri } }
    );

    const stacktrace = requestItem.stacktrace;
    const stackLen = stacktrace ? stacktrace.length : 0;

    if (stack) {
      ok(stacktrace, `Request #${i} has a stacktrace`);
      ok(stackLen > 0,
        `Request #${i} (${causeType}) has a stacktrace with ${stackLen} items`);

      // if "stack" is array, check the details about the top stack frames
      if (Array.isArray(stack)) {
        stack.forEach((frame, j) => {
          is(stacktrace[j].functionName, frame.fn,
            `Request #${i} has the correct function on JS stack frame #${j}`);
          is(stacktrace[j].filename.split("/").pop(), frame.file,
            `Request #${i} has the correct file on JS stack frame #${j}`);
          is(stacktrace[j].lineNumber, frame.line,
            `Request #${i} has the correct line number on JS stack frame #${j}`);
          is(stacktrace[j].asyncCause, frame.asyncCause,
            `Request #${i} has the correct async cause on JS stack frame #${j}`);
        });
      }
    } else {
      is(stackLen, 0, `Request #${i} (${causeType}) has an empty stacktrace`);
    }
  });

  // Sort the requests by cause and check the order
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-list-cause-button"));
  const expectedOrder = EXPECTED_REQUESTS.map(r => r.causeType).sort();
  expectedOrder.forEach((expectedCause, i) => {
    const cause = getSortedRequests(store.getState()).get(i).cause.type;
    is(cause, expectedCause, `The request #${i} has the expected cause after sorting`);
  });

  await teardown(monitor);
});
