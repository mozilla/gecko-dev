/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests for path column. Note that the column
 * header is visible only if there are requests in the list.
 */
add_task(async function () {
  const { monitor, tab } = await initNetMonitor(SIMPLE_URL, {
    requestCount: 1,
  });
  const { document } = monitor.panelWin;
  info("Starting test... ");

  const onNetworkEvents = waitForNetworkEvents(monitor, 2);
  await reloadBrowser();
  await ContentTask.spawn(tab.linkedBrowser, null, () => {
    content.wrappedJSObject.fetch("data:text/plain,some_text");
  });
  await onNetworkEvents;

  await showColumn(monitor, "path");

  const pathColumn = document.querySelector(`.requests-list-path`);
  const requestList = document.querySelectorAll(
    ".network-monitor .request-list-item"
  );

  ok(pathColumn, "Path column should be visible");
  is(
    requestList[0].querySelector(".requests-list-path div:first-child")
      .textContent,
    "/browser/devtools/client/netmonitor/test/html_simple-test-page.html",
    "Path content should contain the request url without origin"
  );
  is(
    requestList[1].querySelector(".requests-list-path div:first-child")
      .textContent,
    "data:text/plain,some_text",
    "Path content should contain the data url"
  );

  await teardown(monitor);
});
