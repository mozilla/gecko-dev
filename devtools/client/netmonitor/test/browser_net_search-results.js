/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test search match functionality.
 * Search panel is visible and clicking matches shows them in the request details.
 */

add_task(async function () {
  const { tab, monitor } = await initNetMonitor(HTTPS_CUSTOM_GET_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;

  // Action should be processed synchronously in tests.
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  const SEARCH_STRING = "test";
  // Execute two XHRs and wait until they are finished.
  const URLS = [
    HTTPS_SEARCH_SJS + "?value=test1",
    HTTPS_SEARCH_SJS + "?value=test2",
  ];

  const wait = waitForNetworkEvents(monitor, 2);
  await SpecialPowers.spawn(tab.linkedBrowser, [URLS], makeRequests);
  await wait;

  // Open the Search panel
  await store.dispatch(Actions.openSearch());

  // Fill Filter input with text and check displayed messages.
  // The filter should be focused automatically.
  typeInNetmonitor(SEARCH_STRING, monitor);
  EventUtils.synthesizeKey("KEY_Enter");

  // Wait until there are two resources rendered in the results
  await waitForDOMIfNeeded(
    document,
    ".search-panel-content .treeRow.resourceRow",
    2
  );

  const searchMatchContents = document.querySelectorAll(
    ".search-panel-content .treeRow .treeIcon"
  );

  for (let i = searchMatchContents.length - 1; i >= 0; i--) {
    clickElement(searchMatchContents[i], monitor);
  }

  // Wait until there are two resources rendered in the results
  await waitForDOMIfNeeded(
    document,
    ".search-panel-content .treeRow.resultRow",
    12
  );

  // Check the matches
  const matches = document.querySelectorAll(
    ".search-panel-content .treeRow.resultRow"
  );

  await checkSearchResult(
    monitor,
    matches[0],
    "#headers-panel",
    ".url-preview .properties-view",
    ".treeRow",
    [SEARCH_STRING]
  );
  await checkSearchResult(
    monitor,
    matches[1],
    "#headers-panel",
    "#responseHeaders .properties-view",
    ".treeRow.selected",
    [SEARCH_STRING]
  );
  await checkSearchResult(
    monitor,
    matches[2],
    "#headers-panel",
    "#requestHeaders .properties-view",
    ".treeRow.selected",
    [SEARCH_STRING]
  );
  await checkSearchResult(
    monitor,
    matches[3],
    "#cookies-panel",
    "#responseCookies .properties-view",
    ".treeRow.selected",
    [SEARCH_STRING]
  );
  await checkSearchResult(
    monitor,
    matches[4],
    "#response-panel",
    ".CodeMirror-code",
    ".CodeMirror-activeline",
    [SEARCH_STRING]
  );
  await checkSearchResult(
    monitor,
    matches[5],
    "#headers-panel",
    ".url-preview .properties-view",
    ".treeRow",
    [SEARCH_STRING]
  );
  await checkSearchResult(
    monitor,
    matches[6],
    "#headers-panel",
    "#responseHeaders .properties-view",
    ".treeRow.selected",
    [SEARCH_STRING]
  );
  await checkSearchResult(
    monitor,
    matches[7],
    "#headers-panel",
    "#requestHeaders .properties-view",
    ".treeRow.selected",
    [SEARCH_STRING]
  );
  await checkSearchResult(
    monitor,
    matches[8],
    "#headers-panel",
    "#requestHeaders .properties-view",
    ".treeRow.selected",
    [SEARCH_STRING]
  );
  await checkSearchResult(
    monitor,
    matches[9],
    "#cookies-panel",
    "#responseCookies .properties-view",
    ".treeRow.selected",
    [SEARCH_STRING]
  );
  await checkSearchResult(
    monitor,
    matches[10],
    "#cookies-panel",
    "#requestCookies .properties-view",
    ".treeRow.selected",
    [SEARCH_STRING]
  );
  await checkSearchResult(
    monitor,
    matches[11],
    "#response-panel",
    ".CodeMirror-code",
    ".CodeMirror-activeline",
    [SEARCH_STRING]
  );

  await teardown(monitor);
});

/**
 * Test the context menu feature inside the search match functionality.
 * Search panel is visible and right-clicking matches shows the appropriate context-menu's.
 */

add_task(async function () {
  const { tab, monitor } = await initNetMonitor(HTTPS_CUSTOM_GET_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;

  // Action should be processed synchronously in tests.
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  const SEARCH_STRING = "matchingResult";
  const matchingUrls = [
    HTTPS_SEARCH_SJS + "?value=matchingResult1",
    HTTPS_SEARCH_SJS + "?value=matchingResult2",
  ];
  const nonMatchingUrls = [HTTPS_SEARCH_SJS + "?value=somethingDifferent"];

  const wait = waitForNetworkEvents(
    monitor,
    matchingUrls.length + nonMatchingUrls.length
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [matchingUrls], makeRequests);
  await SpecialPowers.spawn(tab.linkedBrowser, [nonMatchingUrls], makeRequests);
  await wait;

  // Open the Search panel
  await store.dispatch(Actions.openSearch());

  // Fill Filter input with text and check displayed messages.
  // The filter should be focused automatically.
  typeInNetmonitor(SEARCH_STRING, monitor);
  EventUtils.synthesizeKey("KEY_Enter");

  // Wait until there are two resources rendered in the results
  await waitForDOMIfNeeded(
    document,
    ".search-panel-content .treeRow.resourceRow",
    2
  );

  const resourceMatches = document.querySelectorAll(
    ".search-panel-content .treeRow .treeIcon"
  );

  // open content matches for first resource:
  const firstResourceMatch = resourceMatches[0];
  clickElement(firstResourceMatch, monitor);

  // Wait until the expanded resource is rendered in the results
  await waitForDOMIfNeeded(
    document,
    ".search-panel-content .treeRow.resultRow",
    1
  );

  // Check the content matches
  const contentMatches = document.querySelectorAll(
    ".search-panel-content .treeRow.resultRow"
  );

  // test contex menu entries for contained content:
  const firstContentMatch = contentMatches[0];
  await checkContentMenuCopy(firstContentMatch, matchingUrls[0], monitor);

  // test the context menu entries for resources
  const secondResourceMatch = resourceMatches[1];
  await checkResourceMenuCopyUrl(secondResourceMatch, matchingUrls[1], monitor);
  await checkResourceMenuResend(secondResourceMatch, monitor);
  await checkResourceMenuBlockUnblock(
    secondResourceMatch,
    matchingUrls[1],
    monitor
  );
  await checkSaveAllAsHARWithContextMenu(
    secondResourceMatch,
    matchingUrls,
    monitor
  );

  // reload tab
  const waitForEvents = waitForNetworkEvents(monitor, 1);
  tab.linkedBrowser.reload();
  await waitForEvents;

  // test that the context menu entries are not available anymore:
  await checkResourceMenuNotAvailbale(secondResourceMatch, monitor);

  await teardown(monitor);
});

async function makeRequests(urls) {
  await content.wrappedJSObject.get(urls[0]);
  await content.wrappedJSObject.get(urls[1]);
  info("XHR Requests executed");
}

async function checkContentMenuCopy(
  contentMatch,
  expectedClipboardValue,
  monitor
) {
  EventUtils.sendMouseEvent({ type: "contextmenu" }, contentMatch);

  // execute the copy command:
  await waitForClipboardPromise(async function setup() {
    await selectContextMenuItem(
      monitor,
      "properties-view-context-menu-copyvalue"
    );
  }, expectedClipboardValue);
}

async function checkResourceMenuCopyUrl(
  resourceMatch,
  expectedClipboardValue,
  monitor
) {
  // trigger context menu:
  EventUtils.sendMouseEvent({ type: "contextmenu" }, resourceMatch);

  // select the context menu entry 'copy-url':
  await waitForClipboardPromise(async function setup() {
    await selectContextMenuItem(monitor, "request-list-context-copy-url");
  }, expectedClipboardValue);
}

async function checkResourceMenuResend(resourceMatch, monitor) {
  const { store, windowRequire } = monitor.panelWin;

  const { getSelectedRequest, getDisplayedRequests } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );

  // expect the appearing of a new request in the list:
  const displayedRequests = getDisplayedRequests(store.getState());
  const originalResourceIds = displayedRequests.map(r => r.id);
  const expectedNrOfRequestsAfterResend = displayedRequests.length + 1;

  // define wait functionality, that waits until a new entry appears with different ID:
  const waitForNewRequest = waitUntil(() => {
    const newResourceId = getSelectedRequest(store.getState())?.id;
    return (
      getDisplayedRequests(store.getState()).length ==
        expectedNrOfRequestsAfterResend &&
      !originalResourceIds.includes(newResourceId)
    );
  });

  // click the context menu 'resend'
  EventUtils.sendMouseEvent({ type: "contextmenu" }, resourceMatch);
  await selectContextMenuItem(monitor, "request-list-context-resend-only");
  await waitForNewRequest;
}

async function checkResourceMenuBlockUnblock(resourceMatch, blockUrl, monitor) {
  const { store, windowRequire } = monitor.panelWin;

  // block resource:
  await toggleBlockedUrl(resourceMatch, monitor, store);

  // assert that there is now 1 blocked URL:
  is(
    store.getState().requestBlocking.blockedUrls.length,
    1,
    "There should be 1 blocked URL"
  );
  is(
    store.getState().requestBlocking.blockedUrls[0].url,
    blockUrl,
    `The blocked URL should be '${blockUrl}'`
  );

  // Open the Search panel again
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  await store.dispatch(Actions.openSearch());

  // block resource:
  await toggleBlockedUrl(resourceMatch, monitor, store, "unblock");

  // assert that there is no blocked URL anymore:
  is(
    store.getState().requestBlocking.blockedUrls.length,
    0,
    "There should be no blocked URL"
  );
}

async function checkSaveAllAsHARWithContextMenu(
  resourceMatch,
  expectedUrls,
  monitor
) {
  const { HarMenuUtils } = monitor.panelWin.windowRequire(
    "devtools/client/netmonitor/src/har/har-menu-utils"
  );

  EventUtils.sendMouseEvent({ type: "mousedown" }, resourceMatch);
  EventUtils.sendMouseEvent({ type: "contextmenu" }, resourceMatch);

  info("Trigger Copy All As HAR from the context menu");
  const onHarCopyDone = HarMenuUtils.once("copy-all-as-har-done");
  await selectContextMenuItem(monitor, "request-list-context-copy-all-as-har");
  const jsonString = await onHarCopyDone;
  info("exported JSON:\n" + jsonString);
  const parsedJson = JSON.parse(jsonString);

  is(
    parsedJson?.log?.entries?.length,
    expectedUrls.length,
    "Expected length of " + expectedUrls.length
  );
  for (let i = 0; i < expectedUrls.length; i++) {
    is(
      parsedJson.log.entries[i].request?.url,
      expectedUrls[i],
      "Expected url was '" + expectedUrls[i] + "'"
    );
  }
}

async function checkResourceMenuNotAvailbale(resourceMatch, monitor) {
  // trigger context menu:
  EventUtils.sendMouseEvent({ type: "contextmenu" }, resourceMatch);

  is(
    !!getContextMenuItem(
      monitor,
      "simple-view-context-menu-request-not-available-anymore"
    ),
    true,
    "context menu item 'not-available' should be present"
  );
  is(
    !!getContextMenuItem(monitor, "request-list-context-resend-only"),
    false,
    "context menu item 'resend' should not be present"
  );
  is(
    !!getContextMenuItem(monitor, "request-list-context-copy-all-as-har"),
    false,
    "context menu item 'copy all as HAR' should not be present"
  );
  is(
    !!getContextMenuItem(monitor, "netmonitor.context.blockURL"),
    false,
    "context menu item 'block URL' should not be present"
  );
}

/**
 * Check whether the search result is correctly linked with the related information
 */
async function checkSearchResult(
  monitor,
  match,
  panelSelector,
  panelContentSelector,
  panelDetailSelector,
  expected
) {
  const { document } = monitor.panelWin;

  // Scroll the match into view so that it's clickable
  match.scrollIntoView();

  // Click on the match to show it
  clickElement(match, monitor);

  console.log(`${panelSelector} ${panelContentSelector}`);
  await waitFor(() =>
    document.querySelector(`${panelSelector} ${panelContentSelector}`)
  );

  const tabpanel = document.querySelector(panelSelector);
  const content = tabpanel.querySelectorAll(
    `${panelContentSelector} ${panelDetailSelector}`
  );

  is(
    content.length,
    expected.length,
    `There should be ${expected.length} item${
      expected.length === 1 ? "" : "s"
    } displayed in this tabpanel`
  );

  // Make sure only 1 item is selected
  if (panelDetailSelector === ".treeRow.selected") {
    const selectedElements = tabpanel.querySelectorAll(panelDetailSelector);
    is(
      selectedElements.length,
      1,
      `There should be only 1 item selected, found ${selectedElements.length} items selected`
    );
  }

  if (content.length === expected.length) {
    for (let i = 0; i < expected.length; i++) {
      is(
        content[i].textContent.includes(expected[i]),
        true,
        `Content must include ${expected[i]}`
      );
    }
  }
}
