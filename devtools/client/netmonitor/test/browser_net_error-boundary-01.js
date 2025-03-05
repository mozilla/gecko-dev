/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Test that top-level net monitor error boundary catches child errors.
 */
add_task(async function () {
  await pushPref("devtools.netmonitor.persistlog", true);
  const { monitor } = await initNetMonitor(SIMPLE_URL, {
    requestCount: 1,
  });

  const { store, windowRequire, document } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  // Intentionally damage the store to cause a child component error
  const state = store.getState();

  // Do NOT nullify state.ui as it is used by the App.js component which is not
  // wrapped in the AppErrorBoundary component.
  // Do NOT nullify state.requests as it will just bypass rendering the requests
  // list.
  // requestBlocking should make the RequestListContent.js component throw when
  // rendering the new request sent right after.
  // In general, this test is very much linked to the specific implementation of
  // the components and might break if said implementation changes.
  state.requestBlocking = null;

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [SIMPLE_URL], url => {
    content.fetch(url);
  });

  // Wait for the panel to fall back to the error UI
  const errorPanel = await waitUntil(() =>
    document.querySelector(".app-error-panel")
  );

  is(errorPanel, !undefined);
  return teardown(monitor);
});
