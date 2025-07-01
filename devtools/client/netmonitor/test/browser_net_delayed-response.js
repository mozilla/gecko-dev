/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that for delayed responses the network details are displayed
 * before the response is complete.
 */
function setupTestServer() {
  const httpServer = createTestHTTPServer();
  httpServer.registerContentType("html", "text/html");
  httpServer.registerPathHandler("/index.html", function (request, response) {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(
      `<!DOCTYPE html>
        <html>
          <body>Test for delayed responses</body>
        </html>
      `
    );
  });

  const { promise, resolve } = Promise.withResolvers();
  let delayedResponse;
  httpServer.registerPathHandler(
    "/delayed-response",
    function (request, response) {
      response.processAsync();
      delayedResponse = response;
      resolve();
    }
  );

  const resolveResponse = () => {
    delayedResponse.setStatusLine(delayedResponse.httpVersion, 200, "OK");
    delayedResponse.setHeader("Content-Type", "text/plain", false);
    delayedResponse.write("Here is some content");
    delayedResponse.finish();
  };
  return { httpServer, responseCreated: promise, resolveResponse };
}

add_task(async function () {
  const { httpServer, responseCreated, resolveResponse } = setupTestServer();
  const port = httpServer.identity.primaryPort;

  const { tab, monitor } = await initNetMonitor(
    `http://localhost:${port}/index.html`,
    { requestCount: 1 }
  );
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  const URL = `http://localhost:${port}/delayed-response`;

  info("Start fetching delayed content");
  const waitForRequest = waitUntil(() =>
    document.querySelector(".request-list-item")
  );
  SpecialPowers.spawn(tab.linkedBrowser, [URL], async url => {
    await content.wrappedJSObject.fetch(url);
  });
  await Promise.all([responseCreated, waitForRequest]);

  let request = document.querySelector(".request-list-item");

  // Should be available when the network event is created
  info("Assert that the URL is in the column");
  is(
    request.querySelector(".requests-list-url")?.textContent,
    URL,
    "The URL of the request should be displayed"
  );

  // Should be available on the `response start` event
  info("Assert that the status is not yet in the column");
  is(
    request.querySelector(".requests-list-status-code")?.textContent,
    undefined,
    "The status of the request should not be displayed"
  );

  info("Open the headers panel and check that the panel is not empty");
  let waitForHeadersPanel = waitForDOM(document, "#headers-panel");
  store.dispatch(Actions.toggleNetworkDetails());
  EventUtils.sendMouseEvent({ type: "mousedown" }, request);
  await waitForHeadersPanel;

  info("Assert that the status is not yet in the column");
  is(
    request.querySelector(".requests-list-status-code")?.textContent,
    undefined,
    "The status of the request should not yet be displayed"
  );

  info("Wait for the response headers");
  await waitForDOM(document, "#requestHeaders");

  ok(
    !!document.querySelectorAll("#requestHeaders .treeRow").length,
    "The list of request headers should be visible"
  );

  ok(
    !document.querySelectorAll("#responseHeaders .treeRow").length,
    "The list of response headers should not be visible yet"
  );

  info("Open the response panel and wait for the response content");
  let waitForResponsePanel = waitForDOM(
    document,
    "#response-panel .empty-notice"
  );
  clickOnSidebarTab(document, "response");
  await waitForResponsePanel;

  is(
    document.querySelector("#response-panel .empty-notice").innerText,
    "No response data available for this request",
    "The response text is not available yet"
  );

  info("Switch back to the headers panel");
  waitForHeadersPanel = waitForDOM(document, "#headers-panel");
  clickOnSidebarTab(document, "headers");
  await waitForHeadersPanel;

  info("Complete the response");
  resolveResponse();

  info("Wait for the response headers");
  await waitForDOM(document, "#responseHeaders");

  info("Assert that the status is now in the column");
  request = document.querySelector(".request-list-item");
  is(
    request.querySelector(".requests-list-status-code").textContent,
    "200",
    "The status of the request should be displayed"
  );

  ok(
    !!document.querySelectorAll("#requestHeaders .treeRow").length,
    "The list of request headers should still be visible"
  );

  ok(
    !!document.querySelectorAll("#responseHeaders .treeRow").length,
    "The list of response headers should now be visible"
  );

  info("Open the response panel and wait for the response content");
  waitForResponsePanel = waitForDOM(
    document,
    "#response-panel .CodeMirror-code"
  );
  clickOnSidebarTab(document, "response");
  await waitForResponsePanel;

  is(
    getCodeMirrorValue(monitor),
    "Here is some content",
    "The text shown in the source editor is correct."
  );

  await teardown(monitor);
});
