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
  const tab = await testSetup(TEST_URL);
  const messageManager = tab.linkedBrowser.messageManager;

  // Open against options to avoid noise from tools
  const toolbox = await openToolbox("options");

  const { resourceCommand, tracerCommand } = toolbox.commands;

  // Observe incoming trace to know when we receive the last expected trace
  // i.e. call to "c" function
  const { promise, resolve } = Promise.withResolvers();
  function onAvailable(resources) {
    const tracedLastFunctionCall = resources.some(resource => {
      const type = resource.shift();
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
  await tracerCommand.toggle();

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

  // Cleanup this trace before tracing another time with the UI
  await tracerCommand.toggle();
  await resourceCommand.unwatchResources(
    [resourceCommand.TYPES.JSTRACER_TRACE],
    {
      onAvailable,
    }
  );
  await resourceCommand.clearResources([resourceCommand.TYPES.JSTRACER_TRACE]);

  // Switch to the second part of this test, covering the UI performance
  const { hud } = await toolbox.selectTool("webconsole");
  await tracerCommand.toggle();
  test = runTest("jstracer.ui-performance.DAMP");
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

  await closeToolbox();
  await testTeardown();
};
