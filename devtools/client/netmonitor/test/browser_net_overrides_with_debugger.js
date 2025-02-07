/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from network-overrides-test-helpers.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "network-overrides-test-helpers.js",
  this
);

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/debugger/test/mochitest/shared-head.js",
  this
);

/**
 * Test network override updates between the network monitor and the debugger.
 */

// From Debugger to Netmonitor:
// - add override in the debugger
// - switch to netmonitor
// - check override is displayed and remove it
// - switch back to debugger and check the override is removed
add_task(async function setOverrideInDebugger_removeOverrideInNetmonitor() {
  const { monitor, document } = await setupNetworkOverridesTest();
  const toolbox = monitor.toolbox;

  info("Switch to debugger and setup an override for a script");
  await toolbox.selectTool("jsdebugger");
  const dbg = createDebuggerContext(toolbox);

  await waitForSourcesInSourceTree(dbg, ["script.js"], {
    noExpand: false,
  });

  let overrides = [
    ...findAllElementsWithSelector(dbg, ".has-network-override"),
  ];
  is(overrides.length, 0, "No override is displayed in the debugger");

  info("Select script.js tree node, and add override");
  await selectSourceFromSourceTree(
    dbg,
    "script.js",
    3,
    "Select the `script.js` script for the tree"
  );

  const path = prepareFilePicker("script-override.js", window);
  await triggerSourceTreeContextMenu(
    dbg,
    findSourceNodeWithText(dbg, "script.js"),
    "#node-menu-overrides"
  );
  await writeTextContentToPath(OVERRIDDEN_SCRIPT, path);

  overrides = [...findAllElementsWithSelector(dbg, ".has-network-override")];
  is(overrides.length, 1, "An override is now displayed in the debugger");

  info("Switch back to netmonitor and check if the script is overridden");
  await toolbox.selectTool("netmonitor");
  const scriptRequest = document.querySelectorAll(".request-list-item")[1];

  // Assert override column is checked but disabled in context menu
  await assertOverrideColumnStatus(monitor, { visible: true });
  assertOverrideCellStatus(scriptRequest, { overridden: true });
  const overrideCell = scriptRequest.querySelector(".requests-list-override");
  ok(
    overrideCell.getAttribute("title").includes("script-override.js"),
    "The override icon's title contains the overridden path"
  );

  // Remove Network override
  await removeNetworkOverride(monitor, scriptRequest);
  ok(
    !scriptRequest.querySelector(".requests-list-override"),
    "There is no override cell"
  );

  info("Switch back to debugger and check the override was removed");
  await toolbox.selectTool("jsdebugger");
  overrides = [...findAllElementsWithSelector(dbg, ".has-network-override")];
  is(overrides.length, 0, "No override is displayed in the debugger");

  info("Switch back to netmonitor and tear down the test");
  await toolbox.selectTool("netmonitor");

  return teardown(monitor);
});

// From Netmonitor to Debugger:
// - add override in the netmonitor
// - switch to debugger
// - check override is displayed and remove it
// - switch back to netmonitor and check the override is removed
add_task(async function setOverrideInNetmonitor_removeOverrideInDebugger() {
  const { monitor, document } = await setupNetworkOverridesTest();
  const toolbox = monitor.toolbox;

  let scriptRequest = document.querySelectorAll(".request-list-item")[1];

  info("Set a network override for the script request");
  const overrideFileName = "script-override.js";
  await setNetworkOverride(
    monitor,
    scriptRequest,
    overrideFileName,
    OVERRIDDEN_SCRIPT
  );

  info("Check that the script request is displayed as overridden");
  // Assert override column is checked but disabled in context menu
  await assertOverrideColumnStatus(monitor, { visible: true });
  assertOverrideCellStatus(scriptRequest, { overridden: true });

  info("Switch to debugger and check the override for a script");
  await toolbox.selectTool("jsdebugger");
  const dbg = createDebuggerContext(toolbox);

  await waitForSourcesInSourceTree(dbg, ["script.js"], {
    noExpand: false,
  });

  let overrides = [
    ...findAllElementsWithSelector(dbg, ".has-network-override"),
  ];
  is(overrides.length, 1, "An override is displayed in the debugger");

  info("Select script.js tree node, and remove the override");
  await selectSourceFromSourceTree(
    dbg,
    "script.js",
    3,
    "Select the `script.js` script for the tree"
  );

  const removed = waitForDispatch(dbg.toolbox.store, "REMOVE_NETWORK_OVERRIDE");
  await triggerSourceTreeContextMenu(
    dbg,
    findSourceNodeWithText(dbg, "script.js"),
    "#node-menu-overrides"
  );
  await removed;

  overrides = [...findAllElementsWithSelector(dbg, ".has-network-override")];
  is(
    overrides.length,
    0,
    "The override is no longer displayed in the debugger"
  );

  info("Switch back to netmonitor and check if the script is overridden");
  await toolbox.selectTool("netmonitor");
  scriptRequest = document.querySelectorAll(".request-list-item")[1];

  // Assert override column is now hidden
  await assertOverrideColumnStatus(monitor, { visible: false });

  return teardown(monitor);
});
