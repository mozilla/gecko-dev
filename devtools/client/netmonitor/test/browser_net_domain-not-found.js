/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the request for a domain that is not found shows
 * correctly.
 */
add_task(async function () {
  const URL = "https://not-existed.com/";
  const { monitor } = await initNetMonitor(URL, {
    requestCount: 1,
    waitForLoad: false,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  // Bug 1932818: In theory we should only get 1 request here, but on neterror,
  // resource://gre-resources/html.css is loading an additional font as data-url
  // which shows up in the netmonitor. Bug filed to find a way to filter it out.
  // In the meantime, filter out non-https schemes from this test.
  store.dispatch(Actions.setRequestFilterText("scheme:https"));

  const wait = waitForNetworkEvents(monitor, 2);
  reloadBrowser({ waitForLoad: false });
  await wait;

  const firstItem = document.querySelectorAll(".request-list-item")[0];

  is(
    firstItem.querySelector(".requests-list-url").innerText,
    URL,
    "The url in the displayed request is correct"
  );
  is(
    firstItem.querySelector(".requests-list-transferred").innerText,
    "NS_ERROR_UNKNOWN_HOST",
    "The error in the displayed request is correct"
  );

  await teardown(monitor);
});
