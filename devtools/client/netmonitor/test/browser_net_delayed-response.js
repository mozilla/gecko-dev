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

  // TODO: Not easy to follow, clean up the pattern after fixing the bug Bug 1557795.
  let resolveResponse;
  const waitForResponse = new Promise(resolve => {
    resolveResponse = resolve;
  });
  httpServer.registerPathHandler(
    "/delayed-response",
    function (request, response) {
      response.processAsync();
      resolveResponse(() => {
        response.setStatusLine(response.httpVersion, 400, "OK");
        response.setHeader("Content-Type", "text/plain", false);
        response.write("Here is some content");
        response.finish();
      });
    }
  );
  return { httpServer, waitForResponse };
}

add_task(async function () {
  // TODO: Enable this test when Bug 1557795 gets fixed.
  // eslint-disable-next-line no-constant-condition
  if (true) {
    return;
  }
  const { httpServer, waitForResponse } = setupTestServer();
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
  await SpecialPowers.spawn(tab.linkedBrowser, URL, async url => {
    await content.wrappedJSObject.fetch(url);
  });
  await waitForRequest;

  const request = document.querySelector(".request-list-item");

  // Should be available when the network event is created
  info("Assert that the URL is in the column");
  is(
    request.querySelector(".requests-list-url")?.textContent,
    URL,
    "The URL of the request should be displayed"
  );

  // Should be available on the `response start` event
  info("Assert that the status is in the column");
  todo_is(
    request.querySelector(".requests-list-status-code")?.textContent,
    "200",
    "The status of the request should be displayed"
  );

  info("Open the headers panel and check that the panel is not empty");
  const waitForHeadersPanel = waitForDOM(document, "#headers-panel");
  store.dispatch(Actions.toggleNetworkDetails());
  EventUtils.sendMouseEvent({ type: "mousedown" }, request);
  await waitForHeadersPanel;

  isnot(
    document.querySelector(".empty-notice")?.innerText,
    "No headers for this request",
    "The headers should not be empty"
  );

  ok(
    !!document.querySelectorAll("#requestHeaders treeRow").length,
    "The list of request headers should be visible"
  );

  ok(
    !document.querySelectorAll("#responseHeaders treeRow").length,
    "The list of response headers should not be visible"
  );

  info("Complete the response");
  (await waitForResponse)();

  info("Wait for the response headers");
  await waitForDOM(document, "#responseHeaders");

  ok(
    !!document.querySelectorAll("#responseHeaders .treeRow").length,
    "The list of response headers should be visible"
  );

  info("Open the response panel and wait for the response content");
  const waitForResponsePanel = waitForDOM(
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
