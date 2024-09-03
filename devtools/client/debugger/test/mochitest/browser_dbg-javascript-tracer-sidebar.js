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
  info("Also enable values recording");
  await toggleJsTracerMenuItem(dbg, "#jstracer-menu-item-log-values");

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
  const tracerTree = await waitForElementWithSelector(
    dbg,
    "#tracer-tab-panel .tree"
  );

  info("Wait for the expected traces to appear in the call tree");
  let traces = await waitFor(() => {
    const elements = tracerTree.querySelectorAll(".trace-line");
    if (elements.length == 3) {
      return elements;
    }
    return false;
  });
  is(traces[0].textContent, "λ main simple1.js:1:16");
  is(traces[1].textContent, "λ foo simple2.js:1:12");
  is(traces[2].textContent, "λ bar simple2.js:3:4");

  info("Select the trace for the call to `foo`");
  EventUtils.synthesizeMouseAtCenter(traces[1], {}, dbg.win);

  let focusedTrace = tracerTree.querySelector(".tree-node.focused .trace-line");
  is(focusedTrace, traces[1], "The clicked trace is now focused");

  // Naive sanity checks for inlines previews
  const inlinePreviews = [
    {
      identifier: "x:",
      value: "1",
    },
    {
      identifier: "y:",
      value: "2",
    },
  ];
  await waitForAllElements(dbg, "inlinePreviewLabels", inlinePreviews.length);

  const labels = findAllElements(dbg, "inlinePreviewLabels");
  const values = findAllElements(dbg, "inlinePreviewValues");
  let index = 0;
  const fnName = "foo";
  for (const { identifier, value } of inlinePreviews) {
    is(
      labels[index].innerText,
      identifier,
      `${identifier} in ${fnName} has correct inline preview label`
    );
    is(
      values[index].innerText,
      value,
      `${identifier} in ${fnName} has correct inline preview value`
    );
    index++;
  }

  // Naive sanity checks for popup previews on hovering
  {
    const { element: popupEl, tokenEl } = await tryHovering(
      dbg,
      1,
      14,
      "previewPopup"
    );
    is(popupEl.querySelector(".objectBox")?.textContent, "1");
    await closePreviewForToken(dbg, tokenEl, "previewPopup");
  }

  {
    const { element: popupEl, tokenEl } = await tryHovering(
      dbg,
      1,
      17,
      "previewPopup"
    );
    is(popupEl.querySelector(".objectBox")?.textContent, "2");
    await closePreviewForToken(dbg, tokenEl, "previewPopup");
  }

  let focusedPausedFrame = findElementWithSelector(
    dbg,
    ".frames .frame.selected"
  );
  ok(!focusedPausedFrame, "Before pausing, there is no selected paused frame");

  info("Trigger a breakpoint");
  const onResumed = SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    async function () {
      content.eval("debugger;");
    }
  );
  await waitForPaused(dbg);
  await waitForSelectedLocation(dbg, 1, 0);

  focusedPausedFrame = findElementWithSelector(dbg, ".frames .frame.selected");
  ok(
    !!focusedPausedFrame,
    "When paused, a frame is selected in the call stack panel"
  );

  focusedTrace = tracerTree.querySelector(".tree-node.focused .trace-line");
  is(focusedTrace, null, "When pausing, there is no trace selected anymore");

  info("Re select the tracer frame while being paused");
  EventUtils.synthesizeMouseAtCenter(traces[1], {}, dbg.win);

  await waitForSelectedLocation(dbg, 1, 12);
  focusedPausedFrame = findElementWithSelector(dbg, ".frames .frame.selected");
  ok(
    !focusedPausedFrame,
    "While paused, if we select a tracer frame, the paused frame is no longer highlighted in the call stack panel"
  );
  const highlightedPausedFrame = findElementWithSelector(
    dbg,
    ".frames .frame.inactive"
  );
  ok(
    !!highlightedPausedFrame,
    "But it is still highlighted as inactive with a grey background"
  );

  await resume(dbg);
  await onResumed;

  // Trigger a click in the content page to verify we do trace DOM events
  BrowserTestUtils.synthesizeMouseAtCenter(
    "button",
    {},
    gBrowser.selectedBrowser
  );

  const clickTrace = await waitFor(() =>
    tracerTree.querySelector(".tracer-dom-event")
  );
  is(clickTrace.textContent, "DOM | click");
  is(
    tracerTree.querySelectorAll(".trace-line").length,
    6,
    "The click event adds two elements in the tree. The DOM Event and its top frame"
  );

  await BrowserTestUtils.synthesizeKey("x", {}, gBrowser.selectedBrowser);
  const keyTrace = await waitFor(() => {
    const elts = tracerTree.querySelectorAll(".tracer-dom-event");
    if (elts.length == 2) {
      return elts[1];
    }
    return false;
  });
  is(keyTrace.textContent, "DOM | keypress");

  is(
    tracerTree.querySelectorAll(".trace-line").length,
    8,
    "The key event adds two elements in the tree. The DOM Event and its top frame"
  );

  info("Trigger a DOM Mutation");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    content.eval(`
      window.doMutation = () => {
        const div = document.createElement("div");
        document.body.appendChild(div);
        //# sourceURL=foo.js
      };
      `);
    content.wrappedJSObject.doMutation();
  });

  // Wait for the `eval` and the `doMutation` calls to be rendered
  traces = await waitFor(() => {
    // Scroll to bottom to ensure rendering the last elements (otherwise they are not because of VirtualizedTree)
    tracerTree.scrollTop = tracerTree.scrollHeight;
    const elements = tracerTree.querySelectorAll(".trace-line");
    // Wait for the expected element to be rendered
    if (
      elements[elements.length - 1].textContent.includes("window.doMutation")
    ) {
      return elements;
    }
    return false;
  });

  const doMutationTrace = traces[traces.length - 1];
  is(doMutationTrace.textContent, "λ window.doMutation eval:2:32");

  // Expand the call to doMutation in order to show the DOM Mutation in the tree
  doMutationTrace.querySelector(".arrow").click();

  const mutationTrace = await waitFor(() =>
    tracerTree.querySelector(".tracer-dom-mutation")
  );
  is(mutationTrace.textContent, "DOM Mutation | add");

  // Click on the mutation trace to open its source
  mutationTrace.click();
  await waitForSelectedSource(dbg, "foo.js");

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
  await toggleJsTracerMenuItem(dbg, "#jstracer-menu-item-log-values");
});
