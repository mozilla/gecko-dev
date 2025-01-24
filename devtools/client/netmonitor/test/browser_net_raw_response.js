/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if showing raw response works and is persisted.
 */

const DEFAULT_RAW_RESPONSE_PREF = "devtools.netmonitor.ui.default-raw-response";

add_task(async function () {
  await pushPref(DEFAULT_RAW_RESPONSE_PREF, false);

  const { tab, monitor } = await initNetMonitor(
    JSON_BASIC_URL + "?name=nogrip",
    {
      requestCount: 1,
    }
  );
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  // Trigger JSON request
  await performRequests(monitor, tab, 1);

  info("selecting first request");
  const firstRequestItem = document.querySelectorAll(".request-list-item")[0];
  EventUtils.sendMouseEvent({ type: "mousedown" }, firstRequestItem);

  info("switching to response panel");
  const waitForRespPanel = waitForDOM(
    document,
    "#response-panel .properties-view"
  );
  const respPanelButton = document.querySelector("#response-tab");
  respPanelButton.click();
  await waitForRespPanel;

  ok(
    !getRawResponseToggle(document).checked,
    "Raw Response toggle isn't checked"
  );
  ok(
    !getRawResponseCodeMirrorElement(document),
    "The CodeMirror instance is not rendered"
  );

  info("Click on toggle to show raw response");
  getRawResponseToggle(document).click();
  await waitFor(() => getRawResponseToggle(document).checked);
  ok(true, "Toggle is now checked");
  await waitFor(() => getRawResponseCodeMirrorElement(document));
  ok(true, "The CodeMirror instance is rendered");
  is(
    Services.prefs.getBoolPref(DEFAULT_RAW_RESPONSE_PREF),
    true,
    "Pref is now true"
  );

  info("Check that the toggle is persisted when navigating within side panels");
  info("Switch to request panel");
  const waitForRequestPanel = waitForDOM(document, "#request-panel");
  document.querySelector("#request-tab").click();
  await waitForRequestPanel;

  info("Switch back to response panel");
  const waitForRawResponsePanel = waitFor(() =>
    getRawResponseCodeMirrorElement(document)
  );
  respPanelButton.click();
  await waitForRawResponsePanel;
  ok(
    true,
    "The CodeMirror instance is rendered when switching back to response panel"
  );
  ok(getRawResponseToggle(document).checked, "Raw toggle is still checked");

  await closeDetailPanel(document);

  info("Reload the page to see the HTML request");
  let waitForHTMLRequest = waitForNetworkEvents(monitor, 1);
  await reloadBrowser();
  await waitForHTMLRequest;

  info("Click on HTML request and wait for raw HTML response to be displayed");
  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]
  );
  await waitFor(() => getRawResponseCodeMirrorElement(document));
  ok(true, "Raw response is displayed");
  ok(!getHtmlPreviewElement(document), "The html preview is not displayed");
  ok(getRawResponseToggle(document).checked, "Raw toggle is still checked");

  info("Click on toggle to hide raw response");
  getRawResponseToggle(document).click();
  await waitFor(() => !getRawResponseToggle(document).checked);
  ok(true, "Toggle is not checked anymore");
  await waitFor(() => !getRawResponseCodeMirrorElement(document));
  ok(true, "CodeMirror editor isn't displayed anymore");
  ok(!!getHtmlPreviewElement(document), "The html preview is displayed");
  is(
    Services.prefs.getBoolPref(DEFAULT_RAW_RESPONSE_PREF),
    false,
    "Pref is now false"
  );

  await closeDetailPanel(document);

  info("Reload the page to see the HTML request");
  waitForHTMLRequest = waitForNetworkEvents(monitor, 1);
  await reloadBrowser();
  await waitForHTMLRequest;

  info(
    "Click on HTML request and wait for rendered HTML response to be displayed"
  );
  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]
  );
  await waitFor(() => getHtmlPreviewElement(document));
  ok(true, "The html preview is displayed");
  ok(
    !getRawResponseCodeMirrorElement(document),
    "The CodeMirror editor is not displayed"
  );
  ok(
    !getRawResponseToggle(document).checked,
    "The Raw toggle is not checked anymore"
  );

  return teardown(monitor);
});

function getRawResponseToggle(doc) {
  return doc.querySelector("#response-panel .devtools-checkbox-toggle");
}

function getRawResponseCodeMirrorElement(doc) {
  return doc.querySelector("#response-panel .CodeMirror");
}

function getHtmlPreviewElement(doc) {
  return doc.querySelector(".html-preview");
}

function closeDetailPanel(doc) {
  doc.querySelector(".sidebar-toggle").click();
  return waitFor(() => !doc.querySelector(".network-details-bar"));
}
