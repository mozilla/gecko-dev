/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  openToolbox,
  closeToolbox,
  testSetup,
  testTeardown,
  runTest,
  waitForDOMElement,
  waitForDOMPredicate,
} = require("damp-test/tests/head");
const {
  waitForConsoleOutputChildListChange,
} = require("damp-test/tests/webconsole/webconsole-helpers");
const {
  TRACER_FIELDS_INDEXES,
} = require("resource://devtools/server/actors/tracer.js");

// Implement a test page trigerring lots of function calls to "a" and "b" function
// before calling "c" function only once.
const TEST_URL = `data:text/html,<!DOCTYPE html><meta charset=utf8><script>
    window.onclick = () => {
      for(let i = 0; i < 100000; i++) {
        a();
        b();
      }
      c();
    };
    function a() {};
    function b() {};
    function c() {};
  </script>`;

module.exports = async function () {
  Services.prefs.setBoolPref(
    "devtools.debugger.features.javascript-tracing",
    true
  );
  const tab = await testSetup(TEST_URL);
  const messageManager = tab.linkedBrowser.messageManager;

  // Open against options to avoid noise from tools for the first server test
  const toolbox = await openToolbox("options");

  await testServerPerformance(messageManager, toolbox);

  await testWebConsolePerformance(messageManager, toolbox);

  await testDebuggerSidebarPerformance(messageManager, toolbox);

  Services.prefs.clearUserPref(
    "devtools.debugger.javascript-tracing-log-method"
  );
  Services.prefs.clearUserPref("devtools.debugger.features.javascript-tracing");

  await closeToolbox();

  await testTeardown();
};

async function testServerPerformance(messageManager, toolbox) {
  Services.prefs.setCharPref(
    "devtools.debugger.javascript-tracing-log-method",
    "console"
  );
  const { resourceCommand } = toolbox.commands;

  // Observe incoming trace to know when we receive the last expected trace
  // i.e. call to "c" function
  const { promise, resolve } = Promise.withResolvers();
  function onAvailable(resources) {
    const tracedLastFunctionCall = resources.some(resource => {
      const type = resource[TRACER_FIELDS_INDEXES.TYPE];
      return (
        type == "frame" && resource[TRACER_FIELDS_INDEXES.FRAME_NAME] == "λ c"
      );
    });
    if (tracedLastFunctionCall) {
      resolve();
    }
  }
  await resourceCommand.watchResources([resourceCommand.TYPES.JSTRACER_TRACE], {
    onAvailable,
  });

  // Start listening for JS traces
  await startTracing(toolbox);

  // The toolbox code will automatically open the console, but close it to avoid rendering the traces
  await toolbox.closeSplitConsole();

  let test = runTest("jstracer.server-performance.DAMP");
  // Trigger a click on the page, to trigger some JS in the test page
  messageManager.loadFrameScript(
    "data:,(" +
      encodeURIComponent(`content.document.documentElement.click()`) +
      ")()",
    true
  );
  // Wait for the last expected trace to be received
  await promise;
  test.done();

  await resourceCommand.unwatchResources(
    [resourceCommand.TYPES.JSTRACER_TRACE],
    {
      onAvailable,
    }
  );

  await stopAndClearTracerData(toolbox);
}

async function testWebConsolePerformance(messageManager, toolbox) {
  const { hud } = await toolbox.selectTool("webconsole");

  // Start tracing to the console
  await startTracing(toolbox);

  const test = runTest("jstracer.webconsole-performance.DAMP");
  // Trigger another click on the page, to trigger some JS in the test page
  messageManager.loadFrameScript(
    "data:,(" +
      encodeURIComponent(`content.document.documentElement.click()`) +
      ")()",
    true
  );
  // Wait for the very last message to become the expected last traced function call to "c"
  await waitForConsoleOutputChildListChange(hud, consoleOutput => {
    const messages = consoleOutput.querySelectorAll(".message-body");
    return (
      messages && messages[messages.length - 1]?.textContent.includes("λ c")
    );
  });
  test.done();

  await stopAndClearTracerData(toolbox);
}

async function testDebuggerSidebarPerformance(messageManager, toolbox) {
  Services.prefs.setCharPref(
    "devtools.debugger.javascript-tracing-log-method",
    "debugger-sidebar"
  );

  const panel = await toolbox.selectTool("jsdebugger");

  // Start tracing to the debugger
  await startTracing(toolbox);

  const test = runTest("jstracer.debugger-sidebar-performance.DAMP");
  // Trigger another click on the page, to trigger some JS in the test page
  messageManager.loadFrameScript(
    "data:,(" +
      encodeURIComponent(`content.document.documentElement.click()`) +
      ")()",
    true
  );
  // Wait for the very last message to become the expected last traced function call to "c"
  dump("Wait for tracer tree\n");
  const traceTree = await waitForDOMElement(
    panel.panelWin.document.body,
    "#tracer-tab-panel .tree"
  );
  dump("Wait for first trace arrow element\n");
  const firstTraceArrow = await waitForDOMElement(
    traceTree,
    ".arrow:not(.open)"
  );
  dump(" got the arrow\n");
  firstTraceArrow.click();

  // Scroll down in the tree and wait for the last expected call to "c" to be visible
  await waitForDOMPredicate(traceTree, function scrollDown() {
    traceTree.scrollBy(0, 1000000);

    // Retrieve the last logged trace in the tree
    // The very last element of the VirtualizedTree (`lastElementChild`)
    // will be an hidden element used by VirtualizedTree to handle the virtual viewport.
    // The element before it, will be the very last TreeNode created by Tracer React Component.
    const lastTreeNode = traceTree.lastElementChild?.previousElementSibling;
    const traceDisplayName = lastTreeNode?.querySelector(
      ".frame-link-function-display-name"
    )?.textContent;

    // Check if the last is the expected one
    if (traceDisplayName?.includes("λ c")) {
      return true;
    }
    return false;
  });
  dump(" Found the last logged tree in the tree");

  test.done();

  await stopAndClearTracerData(toolbox);
}

async function startTracing(toolbox) {
  const { tracerCommand } = toolbox.commands;
  const onTracingActive = new Promise(resolve => {
    tracerCommand.on("toggle", function listener() {
      if (!tracerCommand.isTracingActive) {
        return;
      }
      tracerCommand.off("toggle", listener);
      resolve();
    });
  });
  await tracerCommand.toggle();
  // Wait for the tracer to be active, otherwise, the next toggle may try to toggle it ON again.
  await onTracingActive;
}

async function stopAndClearTracerData(toolbox) {
  const { tracerCommand, resourceCommand } = toolbox.commands;
  // Stop tracing
  await tracerCommand.toggle();

  // Cleanup this trace before tracing another time with another UI
  await resourceCommand.clearResources([resourceCommand.TYPES.JSTRACER_TRACE]);
}
