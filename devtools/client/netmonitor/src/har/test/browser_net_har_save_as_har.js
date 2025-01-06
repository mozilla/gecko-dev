"use strict";

/**
 * Tests for saving selected request in HAR file.
 */
add_task(async function () {
  const { tab, monitor } = await initNetMonitor(
    HAR_EXAMPLE_URL + "html_har_post-data-test-page.html",
    {
      requestCount: 1,
    }
  );

  info("Starting test...");
  const { store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  // Execute one POST request on the page and wait till its done.
  const wait = waitForNetworkEvents(monitor, 1);
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    content.wrappedJSObject.executeTest();
  });
  await wait;

  // save selected request in HAR file.
  const savedHar = await saveAsHARWithContextMenu(monitor);

  // check the saved file content.
  isnot(savedHar.log, null, "The HAR log must exist");
  is(savedHar.log.pages.length, 1, "There must be one page");
  is(savedHar.log.entries.length, 1, "There must be only one request");

  return teardown(monitor);
});
