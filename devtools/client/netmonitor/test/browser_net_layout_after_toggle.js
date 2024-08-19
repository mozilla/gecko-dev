/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the netmonitor UI is not broken after toggling to another panel.
 */

add_task(async function () {
  const { monitor, toolbox } = await initNetMonitor(HTTPS_SIMPLE_URL, {
    requestCount: 1,
  });
  ok(monitor, "The network monitor was opened");

  const onNetworkEvent = waitForNetworkEvents(monitor, 1);
  await navigateTo(HTTPS_SIMPLE_URL);
  await onNetworkEvent;

  const { document } = monitor.panelWin;

  info("Select the first request to show the details side panel");
  const firstRequest = document.querySelectorAll(".request-list-item")[0];
  EventUtils.sendMouseEvent({ type: "mousedown" }, firstRequest);

  // Wait for a reflow before measuring the request list height.
  await new Promise(r =>
    window.requestAnimationFrame(() => TestUtils.executeSoon(r))
  );
  const requestsListHeight = getRequestsListHeight(document);

  info("Select the inspector");
  await toolbox.selectTool("inspector");

  info("Wait for Net panel to be hidden");
  await waitUntil(() => document.visibilityState == "hidden");

  info("Select the Net panel again");
  await toolbox.selectTool("netmonitor");

  info("Wait for Net panel to be hidden");
  await waitUntil(() => document.visibilityState == "visible");

  ("Wait until the requests list has the same height as before");
  await waitFor(
    () => getRequestsListHeight(document) == requestsListHeight,
    "Requests list height is the same after switching to another panel"
  );
  await teardown(monitor);
});

function getRequestsListHeight(document) {
  return document.querySelector(".requests-list-scroll").offsetHeight;
}
