/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if "Learn More" links are correctly displayed
 * next to headers.
 */
add_task(async function testHeadersLearnMoreLink() {
  const { tab, monitor } = await initNetMonitor(POST_DATA_URL, {
    requestCount: 1,
  });
  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  await performRequests(monitor, tab, 2);

  // Open Headers panel for the customized request sent by html_post-data-test-page.html.
  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[1]
  );

  await waitForDOMIfNeeded(document, "#responseHeaders, #requestHeaders", 2);

  testShowLearnMore(document);

  await teardown(monitor);
});

/*
 * Tests that a "Learn More" button is only shown if
 * and only if a header is documented in MDN.
 */
function testShowLearnMore(document) {
  const {
    getHeadersURL,
  } = require("resource://devtools/client/netmonitor/src/utils/doc-utils.js");

  const headerRows = document.querySelectorAll(
    ".properties-view .treeRow.stringRow"
  );
  Assert.greater(headerRows.length, 0);

  for (const rowEl of headerRows) {
    const headerName = rowEl.querySelectorAll(".treeLabelCell .treeLabel")[0]
      .textContent;
    const headerDocURL = getHeadersURL(headerName);
    const learnMoreEl = rowEl.querySelectorAll(
      ".treeValueCell .learn-more-link"
    );
    Assert.equal(
      learnMoreEl.length,
      headerDocURL ? 1 : 0,
      'Only a documented header should include a "Learn More" button'
    );
  }
}
