/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that request blocking prevents requests from reaching the server.
 */

add_task(async function () {
  const httpServer = setupTestServer();
  const port = httpServer.identity.primaryPort;

  const { tab, monitor } = await initNetMonitor(
    `http://localhost:${port}/index.html`,
    {
      requestCount: 1,
    }
  );
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;

  // Action should be processed synchronously in tests
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  // Open the request blocking panel
  store.dispatch(Actions.toggleRequestBlockingPanel());

  // Helper to send a request from the content page to /count and return the
  // count.
  async function fetchCount() {
    return ContentTask.spawn(
      tab.linkedBrowser,
      `http://localhost:${port}/count`,
      async function (url) {
        const response = await content.wrappedJSObject.fetch(url);
        const _count = await response.text();
        return _count * 1;
      }
    );
  }

  info("Send a first request to /count, non-blocked");
  let onNetworkEvent = waitForNetworkEvents(monitor, 1);
  let count = await fetchCount();
  await onNetworkEvent;

  is(count, 1, "Count is set to 1");

  info("Block requests matching the pattern 'count'");
  await addBlockedRequest("count", monitor);

  info("Send several requests to /count, blocked");
  // The bug can be reliably reproduced in some scenarios, but I could not find
  // a consistent way to make it fail with browser mochitests.
  // With 100 requests, the old implementation would usually let a few requests
  // go through.
  const blockedRequestsCount = 100;
  onNetworkEvent = waitForNetworkEvents(monitor, blockedRequestsCount);
  for (let i = 0; i < blockedRequestsCount; i++) {
    try {
      await fetchCount();
    } catch (e) {}
  }
  await onNetworkEvent;

  info("Unblock requests matching the pattern 'count'");
  const requestItems = document.querySelectorAll(".request-list-item");
  EventUtils.sendMouseEvent({ type: "mousedown" }, requestItems[1]);
  await toggleBlockedUrl(requestItems[1], monitor, store, "unblock");

  info("Send a last request to /count, non-blocked");
  onNetworkEvent = waitForNetworkEvents(monitor, 1);
  count = await fetchCount();
  await onNetworkEvent;

  // Count should only be set to 2, because the second request never reached
  // the server.
  is(count, 2, "Count is set to 2");

  await teardown(monitor);
});

function setupTestServer() {
  const httpServer = createTestHTTPServer();
  httpServer.registerContentType("html", "text/html");

  httpServer.registerPathHandler("/index.html", function (request, response) {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(`<!DOCTYPE html>
    <html><body><h1>Test requests blocked before reaching the server
    `);
  });

  let counter = 0;
  httpServer.registerPathHandler("/count", function (request, response) {
    response.setStatusLine(request.httpVersion, 200, "OK");
    counter++;
    response.setHeader("Content-Type", "text/plain", false);
    response.write(counter);
  });

  return httpServer;
}
