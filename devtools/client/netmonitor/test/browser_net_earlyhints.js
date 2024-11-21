/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that early hint status appears to indicate the presence
 * of early hint notifications by the request.
 */

add_task(async function testEarlyHintStatusCode() {
  const { tab, monitor } = await initNetMonitor(STATUS_CODES_URL, {
    requestCount: 1,
  });

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  await performEarlyHintRequest(monitor, tab);
  const EARLYHINT_REQUEST_URL = `sjs_early-hint-test-server.sjs?early-hint-pixel.sjs=5ecccd01-dd3f-4bbd-bd3e-0491d7dd78a1`;

  const earlyRequestItem = document.querySelector(".request-list-item");

  const statusCodes = earlyRequestItem.querySelectorAll(
    ".requests-list-status .status-code"
  );
  is(
    statusCodes.length,
    2,
    "There are two status codes displayed in the status column"
  );

  is(statusCodes[0].innerText, "103", "The first status is 103 early hint");
  is(statusCodes[1].innerText, "200", "The second status is the 200 OK");

  is(
    earlyRequestItem.querySelector(".requests-list-file").innerText,
    EARLYHINT_REQUEST_URL,
    "The url in the displayed request is correct"
  );

  EventUtils.sendMouseEvent({ type: "mousedown" }, earlyRequestItem);

  // Wait till all the summary section is loaded
  await waitUntil(() =>
    document.querySelector("#headers-panel .tabpanel-summary-value")
  );
  const panel = document.querySelector("#headers-panel");
  const earlyHintsStatusCode = panel.querySelector(
    ".headers-earlyhint-status .status-code"
  );

  EventUtils.sendMouseEvent({ type: "mouseover" }, earlyHintsStatusCode);

  is(
    parseInt(earlyHintsStatusCode.dataset.code, 10),
    103,
    "The status summary code is correct."
  );

  await teardown(monitor);
});

add_task(async function testEarlyHintFilter() {
  Services.prefs.setBoolPref("devtools.netmonitor.persistlog", true);
  const { tab, monitor } = await initNetMonitor(STATUS_CODES_URL, {
    requestCount: 1,
  });

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  const waitForEvents = waitForNetworkEvents(monitor, 1);
  tab.linkedBrowser.reload();
  await waitForEvents;

  await performEarlyHintRequest(monitor, tab);
  document.querySelector(".devtools-filterinput").focus();

  EventUtils.sendString("103");
  ok(
    document.querySelector(".devtools-autocomplete-popup"),
    "Autocomplete Popup Created"
  );

  testAutocompleteContents(["status-code:103"], document);
  EventUtils.synthesizeKey("KEY_Enter");
  is(
    document.querySelector(".devtools-filterinput").value,
    "status-code:103",
    "Value correctly set after Enter"
  );

  await waitUntil(
    () => document.querySelectorAll(".request-list-item").length == 1
  );

  const statusCode = document.querySelector(
    ".request-list-item .requests-list-status .status-code"
  );
  is(statusCode.innerText, "103", "The first status is 103 early hint");

  await teardown(monitor);
  Services.prefs.setBoolPref("devtools.netmonitor.persistlog", false);
});

async function performEarlyHintRequest(monitor, tab) {
  const wait = waitForNetworkEvents(monitor, 1);
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    content.wrappedJSObject.performEarlyHintRequest();
  });
  await wait;
}
