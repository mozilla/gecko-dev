/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests the Javascript Tracing feature.

"use strict";

add_task(async function () {
  // This is preffed off for now, so ensure turning it on
  await pushPref("devtools.debugger.features.javascript-tracing", true);

  const dbg = await initDebugger("doc-scripts.html");

  info("Force the log method to be the debugger sidebar");
  await toggleJsTracerMenuItem(dbg, "#jstracer-menu-item-debugger-sidebar");

  info("Enable the tracing");
  await toggleJsTracer(dbg.toolbox);

  is(
    dbg.selectors.getSelectedPrimaryPaneTab(),
    "tracer",
    "The tracer sidebar is automatically shown on start"
  );

  const topLevelThreadActorID =
    dbg.toolbox.commands.targetCommand.targetFront.threadFront.actorID;
  info("Wait for tracing to be enabled");
  await waitForState(dbg, () => {
    return dbg.selectors.getIsThreadCurrentlyTracing(topLevelThreadActorID);
  });

  const tracerMessage = findElementWithSelector(
    dbg,
    "#tracer-tab-panel .tracer-message"
  );
  is(tracerMessage.textContent, "Waiting for the first JavaScript executions");

  invokeInTab("main");

  info("Wait for the call tree to appear in the tracer panel");
  const tree = await waitForElementWithSelector(dbg, "#tracer-tab-panel .tree");

  info("Wait for the expected traces to appear in the call tree");
  const traces = await waitFor(() => {
    const elements = tree.querySelectorAll(".trace-line");
    if (elements.length == 3) {
      return elements;
    }
    return false;
  });
  is(traces[0].textContent, "λ main simple1.js:1:16");
  is(traces[1].textContent, "λ foo simple2.js:1:12");
  is(traces[2].textContent, "λ bar simple2.js:3:4");

  // Trigger a click in the content page to verify we do trace DOM events
  BrowserTestUtils.synthesizeMouseAtCenter(
    "button",
    {},
    gBrowser.selectedBrowser
  );

  const clickTrace = await waitFor(() =>
    tree.querySelector(".tracer-dom-event")
  );
  is(clickTrace.textContent, "DOM | click");

  await BrowserTestUtils.synthesizeKey("x", {}, gBrowser.selectedBrowser);
  const keyTrace = await waitFor(() => {
    const elts = tree.querySelectorAll(".tracer-dom-event");
    if (elts.length == 2) {
      return elts[1];
    }
    return false;
  });
  is(keyTrace.textContent, "DOM | keypress");

  // Assert the final content of the tree before stopping
  const finalTreeSize = 7;
  is(tree.querySelectorAll(".trace-line").length, finalTreeSize);

  // Test Disabling tracing
  info("Disable the tracing");
  await toggleJsTracer(dbg.toolbox);
  info("Wait for tracing to be disabled");
  await waitForState(dbg, () => {
    return !dbg.selectors.getIsThreadCurrentlyTracing(topLevelThreadActorID);
  });

  invokeInTab("inline_script2");

  // Let some time for the tracer to appear if we failed disabling the tracing
  await wait(1000);

  info("Reset back to the default value");
  await toggleJsTracerMenuItem(dbg, "#jstracer-menu-item-console");
});
