/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that delayed chunked responses are displayed correctly.
 * The chunks should be displayed as they are received from the backend.
 */
function setupTestServer() {
  const httpServer = createTestHTTPServer();
  httpServer.registerContentType("html", "text/html");
  httpServer.registerPathHandler("/index.html", function (request, response) {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(
      `<!DOCTYPE html>
      <html>
        <body>Test requests blocked before reaching the server</body>
      </html>
    `
    );
  });

  let chunkedResponse;
  httpServer.registerPathHandler("/chunked-data", function (request, response) {
    response.processAsync();
    response.setStatusLine(request.httpVersion, 400, "OK");
    response.setHeader("Content-Type", "text/plain", false);
    response.setHeader("Transfer-Encoding", "chunked", false);
    response.write("7\r\n");
    response.write("Mozilla\r\n");
    chunkedResponse = response;
  });

  const completeResponse = () => {
    chunkedResponse.write("11\r\n");
    chunkedResponse.write("Developer Network\r\n");
    chunkedResponse.write("0\r\n");
    chunkedResponse.write("\r\n");
    chunkedResponse.finish();
  };

  return { httpServer, completeResponse };
}

add_task(async function () {
  // TODO: Enable test when Bug 1557795 gets fixed.
  // eslint-disable-next-line no-constant-condition
  if (true) {
    return;
  }
  const { httpServer, completeResponse } = setupTestServer();
  const port = httpServer.identity.primaryPort;

  const { tab, monitor } = await initNetMonitor(
    `http://localhost:${port}/index.html`,
    { requestCount: 1 }
  );
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  info("Start fetching chunked content");
  const waitForRequest = waitUntil(() => {
    const request = document.querySelector(".request-list-item");
    return (
      request && request.querySelector(".requests-list-transferred").textContent
    );
  });
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    `http://localhost:${port}/chunked-data`,
    async url => {
      await content.wrappedJSObject.fetch(url);
    }
  );
  await waitForRequest;

  info("Assert the size of the transferred chunk in the column");
  const chunkedRequest = document.querySelector(".request-list-item");
  is(
    chunkedRequest.querySelector(".requests-list-transferred").textContent,
    "154 B",
    "The transferred data should be correct"
  );

  info("Open the response panel and wait for initial chunk of the data");
  const waitForResponsePanel = waitForDOM(
    document,
    "#response-panel .CodeMirror-code"
  );
  store.dispatch(Actions.toggleNetworkDetails());
  clickOnSidebarTab(document, "response");
  await waitForResponsePanel;

  is(
    getCodeMirrorValue(monitor),
    "Mozilla",
    "The text shown in the source editor is correct."
  );

  info("Send the last chunk of data");
  completeResponse();

  info("Wait for the last chunk of the data");
  await waitUntil(() => getCodeMirrorValue(monitor).length > 7);

  info("Assert the size of the all the transferred data in the column");
  is(
    getCodeMirrorValue(monitor),
    "MozillaDeveloper Network",
    "The text shown in the source editor is correct."
  );
  is(
    chunkedRequest.querySelector(".requests-list-transferred").textContent,
    "171 B",
    "The transferred data is correct"
  );

  await teardown(monitor);
});
