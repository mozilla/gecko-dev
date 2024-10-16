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
  getBrowserWindow,
} = require("damp-test/tests/head");
const {
  waitForConsoleOutputChildListChange,
} = require("damp-test/tests/webconsole/webconsole-helpers");
const {
  TRACER_FIELDS_INDEXES,
} = require("resource://devtools/server/actors/tracer.js");
const {
  CommandsFactory,
} = require("devtools/shared/commands/commands-factory");

// Implement a test page trigerring lots of function calls to "a" and "b" function
// before calling "c" function only once.
const TEST_URL = `data:text/html,<!DOCTYPE html><meta charset=utf8><script>
    window.onclick = () => {
      for(let i = 0; i < 30000; i++) {
        a(window, i);
        b(document.body);
      }
      c();
    };
    function a(win, i) { return win; };
    function b(body) { return body; };
    function c() {};
  </script><body>JS Tracer test</body>`;

/**
 * This test tracks the rendering speed of all JavaScript Tracer output methods.
 *
 * Except for `testServerPerformance`, which tracks the raw performance of server
 * and client codebases.
 */

module.exports = async function () {
  Services.prefs.setBoolPref(
    "devtools.debugger.features.javascript-tracing",
    true
  );

  Services.prefs.setBoolPref(
    "devtools.debugger.javascript-tracing-values",
    true
  );

  const tab = await testSetup(TEST_URL);
  const messageManager = tab.linkedBrowser.messageManager;

  await testServerPerformance(tab, messageManager);

  const toolbox = await openToolbox("webconsole");

  await testWebConsoleOutputPerformance(messageManager, toolbox);

  await testDebuggerSidebarOutputPerformance(messageManager, toolbox);

  await testProfilerOutputPerformance(messageManager, toolbox);

  Services.prefs.clearUserPref(
    "devtools.debugger.javascript-tracing-log-method"
  );
  Services.prefs.clearUserPref("devtools.debugger.features.javascript-tracing");

  await closeToolbox();

  await testTeardown();
};

async function testServerPerformance(tab, messageManager) {
  dump("Testing server+client performance\n");
  Services.prefs.setCharPref(
    "devtools.debugger.javascript-tracing-log-method",
    "console"
  );

  const commands = await CommandsFactory.forTab(tab);
  await commands.targetCommand.startListening();
  await commands.tracerCommand.initialize();

  const { resourceCommand } = commands;

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
  await startTracing(commands);

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

  await stopAndClearTracerData(commands);
}

async function testWebConsoleOutputPerformance(messageManager, toolbox) {
  dump("Testing web console output performance\n");
  const { hud } = await toolbox.selectTool("webconsole");

  // Start tracing to the console
  await startTracing(toolbox.commands);

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

  await stopAndClearTracerData(toolbox.commands);
}

async function testDebuggerSidebarOutputPerformance(messageManager, toolbox) {
  dump("Testing debugger sidebar output performance\n");
  Services.prefs.setCharPref(
    "devtools.debugger.javascript-tracing-log-method",
    "debugger-sidebar"
  );

  const panel = await toolbox.selectTool("jsdebugger");

  // Start tracing to the debugger
  await startTracing(toolbox.commands);

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
  dump("Expand the trace tree\n");
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
  dump(" Found the last logged tree in the tree\n");

  test.done();

  await stopAndClearTracerData(toolbox.commands);
}

/**
 * This test is slightly different as this output doesn't show live data.
 * So we first record the impact of background trace recording when executing JS calls (profiler-recording-performance),
 * and then the time it takes to collect all the traces and open the profiler frontend (profiler-collection-performance).
 */
async function testProfilerOutputPerformance(messageManager, toolbox) {
  dump("Testing profiler output performance\n");
  Services.prefs.setCharPref(
    "devtools.debugger.javascript-tracing-log-method",
    "profiler"
  );
  // Talos doesn't support the https protocol on example.com
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  const baseURI = "http://example.com";
  Services.prefs.setCharPref(
    "devtools.performance.recording.ui-base-url",
    baseURI
  );
  const uriPath = "/tests/devtools/addon/content/pages/simple.html?";
  Services.prefs.setCharPref(
    "devtools.performance.recording.ui-base-url-path",
    uriPath
  );

  // Start tracing to the profiler
  await startTracing(toolbox.commands);

  // First record the time it takes to run the observed JS code
  let test = runTest("jstracer.profiler-recording-performance.DAMP");
  // Call `onclick` to trigger some JS activity in the test page
  const onContentCodeExecuted = new Promise(done => {
    messageManager.addMessageListener("executed", done);
  });
  messageManager.loadFrameScript(
    "data:,content.wrappedJSObject.onclick();sendAsyncMessage('executed')",
    true
  );
  dump("Wait for end of JS execution in the content process\n");
  await onContentCodeExecuted;
  dump("JS Execution ended\n");
  test.done();

  // Then the time it takes to open the record in the profiler frontend
  // (we actually ignore the frontend, we only measure the time it takes to open the tab,
  //  which involves collecting all profiler data)
  test = runTest("jstracer.profiler-collection-performance.DAMP");
  const onTabOpened = new Promise(resolve => {
    getBrowserWindow().addEventListener(
      "TabOpen",
      function (event) {
        resolve(event.target);
      },
      { once: true }
    );
  });
  dump("Stop recording and wait for the profiler tab to be opened\n");
  await stopAndClearTracerData(toolbox.commands);
  const profilerTab = await onTabOpened;
  dump("Profiler tab opened\n");
  test.done();

  // Also close that tab to avoid polluting the following tests.
  const { gBrowser } = getBrowserWindow();
  const onTabClosed = new Promise(resolve => {
    getBrowserWindow().addEventListener(
      "TabClose",
      function () {
        resolve();
      },
      { once: true }
    );
  });
  dump("Close the profiler tab\n");
  await gBrowser.removeTab(profilerTab);
  await onTabClosed;

  Services.prefs.clearUserPref("devtools.performance.recording.ui-base-url");
  Services.prefs.clearUserPref(
    "devtools.performance.recording.ui-base-url-path"
  );
}

async function startTracing(commands) {
  const { tracerCommand } = commands;
  if (tracerCommand.isTracingActive) {
    throw new Error("Can't start as tracer is already active");
  }
  const onTracingActive = new Promise(resolve => {
    tracerCommand.on("toggle", async function listener() {
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

async function stopAndClearTracerData(commands) {
  const { tracerCommand, resourceCommand } = commands;
  if (!tracerCommand.isTracingActive) {
    throw new Error("Can't stop as tracer is not active");
  }
  const onTracingStopped = new Promise(resolve => {
    tracerCommand.on("toggle", async function listener() {
      if (tracerCommand.isTracingActive) {
        return;
      }
      tracerCommand.off("toggle", listener);
      resolve();
    });
  });
  // Stop tracing
  await tracerCommand.toggle();
  await onTracingStopped;

  // Cleanup this trace before tracing another time with another UI
  await resourceCommand.clearResources([resourceCommand.TYPES.JSTRACER_TRACE]);
}
