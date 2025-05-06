/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test basic SSE connection.
 */

function setupTestServer() {
  const httpServer = createTestHTTPServer();
  httpServer.registerContentType("html", "text/html");
  httpServer.registerPathHandler("/index.html", function (request, response) {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(
      `<!DOCTYPE html>
      <html>
        <head>
          <title>SSE Inspection Test Page</title>
        </head>
        <body>
            <h1>SSE Inspection Test Page</h1>
            <script type="text/javascript">
            /* exported openConnection, closeConnection */
            "use strict";

            let es;
            function openConnection(endpoint) {
              return new Promise(resolve => {
                es = new EventSource("http://localhost:${httpServer.identity.primaryPort}/" + endpoint);
                es.onmessage = function () {
                  resolve();
                };
              });
            }

            function closeConnection() {
              es.close();
            }
            </script>
        </body>
      </html>
    `
    );
  });

  let sseResponse;
  httpServer.registerPathHandler("/sse", function (request, response) {
    response.processAsync();
    response.setHeader("Content-Type", "text/event-stream");
    response.write("data: Why so serious?\n\n");
    response.write("data: Why so serious?\n\n");
    response.write("data: Why so serious?\n\n");
    response.finish();
  });

  httpServer.registerPathHandler("/sse-delay", function (request, response) {
    response.processAsync();
    response.setHeader("Content-Type", "text/event-stream");
    response.write("data: Why so serious?\n\n");
    sseResponse = response;
  });

  const sendResponseMessages = () => {
    sseResponse.write("data: Another why so serious?\n\n");
    sseResponse.write("data: Another why so serious?\n\n");
    sseResponse.write("data: Another why so serious?\n\n");
  };

  const completeResponse = () => {
    sseResponse.finish();
  };

  return { httpServer, sendResponseMessages, completeResponse };
}

add_task(async function testBasicServerSentEvents() {
  const { httpServer } = setupTestServer();
  const port = httpServer.identity.primaryPort;

  const { tab, monitor } = await initNetMonitor(
    `http://localhost:${port}/index.html`,
    { requestCount: 1 }
  );
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  const onNetworkEvents = waitForNetworkEvents(monitor, 1);
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    await content.wrappedJSObject.openConnection("sse");
  });
  await onNetworkEvents;

  const requests = document.querySelectorAll(".request-list-item");
  is(requests.length, 1, "There should be one request");

  const requestItem = document.querySelectorAll(".request-list-item")[0];

  const type = requestItem.querySelector(".requests-list-type").textContent;
  is(type, "eventsource", "Type should be rendered correctly.");

  // Select the request to open the side panel.
  EventUtils.sendMouseEvent({ type: "mousedown" }, requests[0]);

  // Wait for messages to be displayed in DevTools
  const waitForMessages = waitForDOM(
    document,
    "#messages-view .message-list-table .message-list-item",
    3
  );

  // Test that 'Save Response As' is not in the context menu
  EventUtils.sendMouseEvent({ type: "contextmenu" }, requests[0]);

  ok(
    !getContextMenuItem(monitor, "request-list-context-save-response-as"),
    "The 'Save Response As' context menu item should be hidden"
  );

  // Close context menu.
  const contextMenu = monitor.toolbox.topDoc.querySelector(
    'popupset menupopup[menu-api="true"]'
  );
  const popupHiddenPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popuphidden"
  );
  contextMenu.hidePopup();
  await popupHiddenPromise;

  // Click on the "Response" panel
  clickOnSidebarTab(document, "response");
  await waitForMessages;

  // Get all messages present in the "Response" panel
  const frames = document.querySelectorAll(
    "#messages-view .message-list-table .message-list-item"
  );

  // Check expected results
  is(frames.length, 3, "There should be three messages");

  is(
    frames[0].querySelector(".message-list-payload").textContent,
    // Initial whitespace comes from ColumnData.
    " Why so serious?",
    "Data column shows correct payload"
  );

  await waitForDOMIfNeeded(
    document,
    "#messages-view .msg-connection-closed-message",
    1
  );

  is(
    !!document.querySelector("#messages-view .msg-connection-closed-message"),
    true,
    "Connection closed message should be displayed"
  );

  is(
    document.querySelector(".message-network-summary-count").textContent,
    "3 messages",
    "Correct message count is displayed"
  );

  is(
    document.querySelector(".message-network-summary-total-size").textContent,
    "45 B total",
    "Correct total size should be displayed"
  );

  is(
    !!document.querySelector(".message-network-summary-total-millis"),
    true,
    "Total time is displayed"
  );

  is(
    document.getElementById("frame-filter-menu"),
    null,
    "Toolbar filter menu is hidden"
  );

  await waitForTick();

  EventUtils.sendMouseEvent(
    { type: "contextmenu" },
    document.querySelector(".message-list-headers")
  );

  const columns = ["data", "time", "retry", "size", "eventName", "lastEventId"];
  for (const column of columns) {
    is(
      !!getContextMenuItem(monitor, `message-list-header-${column}-toggle`),
      true,
      `Context menu item "${column}" is displayed`
    );
  }

  return teardown(monitor);
});

/**
 * Test various scenarios around SSE requests,
 * 1) Assert that the SSE requests messages are displayed before the connection is closed.
 * 2) Assert that subsequent messages after the response panel is open are visible.
 * 3) Assert that the close connection message is displayed when the connection is closed.
 */
add_task(async function testServerSentEventsDetails() {
  // TODO: Should enable this test when Bug 1557795 gets fixed.
  // eslint-disable-next-line no-constant-condition
  if (true) {
    return null;
  }
  const { httpServer, sendResponseMessages, completeResponse } =
    setupTestServer();
  const port = httpServer.identity.primaryPort;

  const { tab, monitor } = await initNetMonitor(
    `http://localhost:${port}/index.html`,
    {
      requestCount: 1,
    }
  );
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  // We are expecting an SSE request whose response will remain pending on the server,
  const waitForRequest = waitUntil(() =>
    document.querySelector(".request-list-item")
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    await content.wrappedJSObject.openConnection("sse-delay");
  });
  await waitForRequest;

  const requests = document.querySelectorAll(".request-list-item");
  is(requests.length, 1, "There should be one request");

  // Wait for messages to be displayed in DevTools
  const waitForMessages = waitForDOM(
    document,
    "#messages-view .message-list-table .message-list-item",
    1
  );

  // Select the request to open the side panel.
  EventUtils.sendMouseEvent({ type: "mousedown" }, requests[0]);

  // Click on the "Response" panel
  clickOnSidebarTab(document, "response");
  await waitForMessages;

  // Get all messages present in the "Response" panel
  const frames = document.querySelectorAll(
    "#messages-view .message-list-table .message-list-item"
  );

  // Check expected results
  is(frames.length, 1, "There should be one message");

  is(
    frames[0].querySelector(".message-list-payload").textContent,
    // Initial whitespace comes from ColumnData.
    " Why so serious?",
    "Data column shows correct payload"
  );

  const waitForMoreMessages = waitForDOM(
    document,
    "#messages-view .message-list-table .message-list-item",
    3
  );
  info("Send a couple of more messages");
  sendResponseMessages();
  await waitForMoreMessages;

  ok(
    !document.querySelector("#messages-view .msg-connection-closed-message"),
    "Connection closed message not be should not be displayed"
  );

  // Lets finish the request
  completeResponse();

  const waitForClose = waitForDOM(
    document,
    "#messages-view .msg-connection-closed-message",
    1
  );

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    await content.wrappedJSObject.closeConnection();
  });

  await waitForClose;

  is(
    !!document.querySelector("#messages-view .msg-connection-closed-message"),
    true,
    "Connection closed message should be displayed"
  );

  // Wait for the connection closed event to complete
  await waitForTime(1000);

  return teardown(monitor);
});
