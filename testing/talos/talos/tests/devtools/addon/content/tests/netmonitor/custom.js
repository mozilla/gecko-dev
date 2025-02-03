/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  openToolboxAndLog,
  navigatePageAndLog,
  reloadPageAndLog,
  closeToolboxAndLog,
  runTest,
  testSetup,
  testTeardown,
  PAGES_BASE_URL,
} = require("damp-test/tests/head");

const {
  exportHar,
  waitForNetworkRequests,
} = require("damp-test/tests/netmonitor/netmonitor-helpers");

function getExpectedRequests({
  bigFileRequests,
  postDataRequests,
  xhrRequests,
  dataRequests,
  bigDataRequests,
}) {
  // These other numbers only state how many requests the test do,
  // we have to keep them in sync with netmonitor.html static content
  const expectedSyncCssRequests = 10,
    expectedSyncJSRequests = 10;

  // In theory we should always expect 30 requests for iframes here:
  // - 10 for html documents
  // - 10 for js files
  // - 10 for css files
  // However even if network events for cached CSS files are supported since
  // Bug 1916960, they are sometimes missing. To avoid timeouts we will only
  // expect 1 here. This should not make a huge timing difference in most tests
  // triggering many requests.
  const expectedSyncIframeRequests = 2 * 10 + 1;

  return (
    1 + // This is for the top level index.html document
    expectedSyncCssRequests +
    expectedSyncJSRequests +
    expectedSyncIframeRequests +
    bigFileRequests +
    postDataRequests +
    xhrRequests +
    dataRequests +
    bigDataRequests
  );
}

function getTestUrl({
  bigFileRequests,
  postDataRequests,
  xhrRequests,
  dataRequests,
  bigDataRequests,
}) {
  return (
    PAGES_BASE_URL +
    "custom/netmonitor/index.html" +
    `?bigFileRequests=${bigFileRequests}` +
    `&postDataRequests=${postDataRequests}` +
    `&xhrRequests=${xhrRequests}` +
    `&dataRequests=${dataRequests}` +
    `&bigDataRequests=${bigDataRequests}`
  );
}

function waitForRequests(tab) {
  const { messageManager } = tab.linkedBrowser;
  const onReady = new Promise(done => {
    messageManager.addMessageListener("ready", done);
  });
  messageManager.loadFrameScript(
    "data:,(" +
      encodeURIComponent(
        `function () {
      if (content.wrappedJSObject.isReady) {
        sendAsyncMessage("ready");
      } else {
        content.addEventListener("message", function () {
          sendAsyncMessage("ready");
        });
      }
    }`
      ) +
      ")()",
    true
  );
  return onReady;
}

module.exports = async function () {
  // These numbers control the number of requests performed by the page.
  let requests = {
    bigFileRequests: 20,
    postDataRequests: 20,
    xhrRequests: 50,
    dataRequests: 0,
    bigDataRequests: 0,
  };

  let tab = await testSetup(getTestUrl(requests));
  let expectedRequests = getExpectedRequests(requests);
  const onReady = waitForRequests(tab);

  // We wait for a custom "ready" event in order to ensure all the requests
  // done during document load are finished before opening the netmonitor.
  // Otherwise some still pending requests will be displayed on toolbox open
  // and the number of requests being displayed will be random and introduce noise
  // in custom.netmonitor.open
  dump("Waiting for document to be ready and have sent all its requests\n");
  await onReady;

  let toolbox = await openToolboxAndLog("custom.netmonitor", "netmonitor");

  // Waterfall will only work after an idle event. Its width is only set after idle.
  // Before that, it doesn't render.
  dump("Waiting for idle in order to ensure running reload with a waterfall\n");
  let window = toolbox.getCurrentPanel().panelWin;
  await new Promise(done => {
    window.requestIdleCallback(done);
  });

  let requestsDone = waitForNetworkRequests(
    "custom.netmonitor",
    toolbox,
    expectedRequests,
    expectedRequests
  );
  await reloadPageAndLog("custom.netmonitor", toolbox);
  await requestsDone;

  await exportHar("custom.netmonitor", toolbox);

  // Fill the request list with many simple requests.
  dump("Test panel performance when the request list contains many requests\n");

  // For this test, we only want data requests, so they can fill the requests
  // list quickly without doing actual network requests.
  requests = {
    bigFileRequests: 0,
    postDataRequests: 0,
    xhrRequests: 0,
    dataRequests: 2000,
    bigDataRequests: 0,
  };
  expectedRequests = getExpectedRequests(requests);

  requestsDone = waitForNetworkRequests(
    "custom.netmonitor.manyrequests",
    toolbox,
    expectedRequests,
    expectedRequests
  );
  await navigatePageAndLog(
    getTestUrl(requests),
    "custom.netmonitor.manyrequests",
    toolbox
  );
  await requestsDone;

  // The main test here is to toggle the visibility of the network panel
  // to check how long it takes for it to hide and show with many requests.
  // See https://bugzilla.mozilla.org/show_bug.cgi?id=1942149.
  let test = runTest("custom.netmonitor.manyrequests.togglepanel");
  await toolbox.selectTool("options");
  await toolbox.selectTool("netmonitor");
  test.done();

  // Test handling of big data URIs.
  dump("Test panel performance with huge data URI requests\n");

  const bigDataRequestsCount = 3;
  requests = {
    bigFileRequests: 0,
    postDataRequests: 0,
    xhrRequests: 0,
    dataRequests: 0,
    bigDataRequests: bigDataRequestsCount,
  };
  expectedRequests = getExpectedRequests(requests);

  requestsDone = waitForNetworkRequests(
    "custom.netmonitor.bigdatarequests",
    toolbox,
    expectedRequests,
    expectedRequests,
    // Additionally, since this test only emits 3 custom requests, but each
    // being slow to process, make sure all data URIs were received.
    function isBigDataRequestTestFinished(requests) {
      const dataRequests = requests.filter(r => r.urlDetails.scheme == "data");
      return dataRequests.length == bigDataRequestsCount;
    }
  );
  await navigatePageAndLog(
    getTestUrl(requests),
    "custom.netmonitor.bigdatarequests",
    toolbox
  );
  await requestsDone;

  await closeToolboxAndLog("custom.netmonitor", toolbox);

  // Bug 1503822, wait one second on test end to prevent a crash during firefox shutdown.
  await new Promise(r => setTimeout(r, 1000));

  await testTeardown();
};
