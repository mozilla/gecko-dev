/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that filter input keeps its value when host or panel changes
 */

add_task(async function () {
  const { monitor } = await initNetMonitor(FILTERING_URL, { requestCount: 1 });
  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  info("Starting test... ");

  const toolbars = document.querySelector("#netmonitor-toolbar-container");

  document.querySelector(".devtools-filterinput").focus();
  EventUtils.sendString("hello");

  await monitor.toolbox.switchHost("right");
  await waitFor(
    () => toolbars.querySelectorAll(".devtools-toolbar").length == 2
  );

  is(
    toolbars.querySelectorAll(".devtools-toolbar").length,
    2,
    "Should be in 2 toolbar mode"
  );

  let input = toolbars.querySelector(".devtools-filterinput");
  is(
    input.value,
    "hello",
    "Value should be preserved after switching to right host"
  );

  await monitor.toolbox.switchHost("bottom");

  input = toolbars.querySelector(".devtools-filterinput");
  is(
    input.value,
    "hello",
    "Value should be preserved after switching to bottom host"
  );

  await monitor.toolbox.selectTool("inspector");
  await monitor.toolbox.selectTool("netmonitor");

  input = toolbars.querySelector(".devtools-filterinput");
  is(input.value, "hello", "Value should be preserved after switching tools");

  await teardown(monitor);
});

/**
 * Test that filter input persists in a new instance of devtools
 */

add_task(async function () {
  const tab = await addTab(FILTERING_URL, { waitForLoad: false });

  const toolbox = await gDevTools.showToolboxForTab(tab, {
    toolId: "netmonitor",
  });
  info("Network monitor pane shown successfully.");

  const monitor = toolbox.getCurrentPanel();

  const toolbars = monitor.panelWin.document.querySelector(
    "#netmonitor-toolbar-container"
  );
  const input = toolbars.querySelector(".devtools-filterinput");
  is(
    input.value,
    "hello",
    "Value persist after closing and reopening devtools"
  );

  await teardown(monitor);
});
