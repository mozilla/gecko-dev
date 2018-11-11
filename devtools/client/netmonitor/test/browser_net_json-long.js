/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if very long JSON responses are handled correctly.
 */

add_task(async function() {
  const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");

  const { tab, monitor } = await initNetMonitor(JSON_LONG_URL);
  info("Starting test... ");

  // This is receiving over 80 KB of json and will populate over 6000 items
  // in a variables view instance. Debug builds are slow.
  requestLongerTimeout(4);

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const {
    getDisplayedRequests,
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  // Execute requests.
  await performRequests(monitor, tab, 1);

  const requestItem = document.querySelector(".request-list-item");
  const requestsListStatus = requestItem.querySelector(".status-code");
  EventUtils.sendMouseEvent({ type: "mouseover" }, requestsListStatus);
  await waitUntil(() => requestsListStatus.title);

  verifyRequestItemTarget(
    document,
    getDisplayedRequests(store.getState()),
    getSortedRequests(store.getState()).get(0),
    "GET",
    CONTENT_TYPE_SJS + "?fmt=json-long",
    {
      status: 200,
      statusText: "OK",
      type: "json",
      fullMimeType: "text/json; charset=utf-8",
      size: L10N.getFormatStr("networkMenu.sizeKB",
        L10N.numberWithDecimals(85975 / 1024, 2)),
      time: true,
    });

  wait = waitForDOM(document, "#response-panel .CodeMirror-code");
  store.dispatch(Actions.toggleNetworkDetails());
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#response-tab"));
  await wait;

  testResponseTab();

  await teardown(monitor);

  function testResponseTab() {
    const tabpanel = document.querySelector("#response-panel");

    is(tabpanel.querySelector(".response-error-header") === null, true,
      "The response error header doesn't have the intended visibility.");
    const jsonView = tabpanel.querySelector(".tree-section .treeLabel") || {};
    is(jsonView.textContent === L10N.getStr("jsonScopeName"), true,
      "The response json view has the intended visibility.");
    is(tabpanel.querySelector(".editor-row-container").clientHeight !== 0, true,
       "The source editor container has visible height.");
    is(tabpanel.querySelector(".CodeMirror-code") === null, false,
      "The response editor has the intended visibility.");
    is(tabpanel.querySelector(".response-image-box") === null, true,
      "The response image box doesn't have the intended visibility.");

    is(tabpanel.querySelectorAll(".tree-section").length, 2,
      "There should be 2 tree sections displayed in this tabpanel.");
    is(tabpanel.querySelectorAll(".treeRow:not(.tree-section)").length, 2047,
      "There should be 2047 json properties displayed in this tabpanel.");
    is(tabpanel.querySelectorAll(".empty-notice").length, 0,
      "The empty notice should not be displayed in this tabpanel.");

    is(tabpanel.querySelector(".tree-section .treeLabel").textContent,
      L10N.getStr("jsonScopeName"),
      "The json view section doesn't have the correct title.");

    const labels = tabpanel
      .querySelectorAll("tr:not(.tree-section) .treeLabelCell .treeLabel");
    const values = tabpanel
      .querySelectorAll("tr:not(.tree-section) .treeValueCell .objectBox");

    is(labels[0].textContent, "0",
      "The first json property name was incorrect.");
    is(values[0].textContent, "{\u2026}",
      "The first json property value was incorrect.");

    is(labels[1].textContent, "1",
      "The second json property name was incorrect.");
    is(values[1].textContent, "{\u2026}",
      "The second json property value was incorrect.");
  }
});
