/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests the Javascript Tracing feature.

"use strict";

const TEST_URL =
  "data:text/html," +
  encodeURIComponent(
    `<script>function main(){foo({a: 1}); foo({b: 2})}; function foo(arg){}</script>`
  );

add_task(async function () {
  // This is preffed off for now, so ensure turning it on
  await pushPref("devtools.debugger.features.javascript-tracing", true);

  const dbg = await initDebuggerWithAbsoluteURL(TEST_URL);

  info("Force the log method to be the debugger sidebar");
  await toggleJsTracerMenuItem(dbg, "#jstracer-menu-item-debugger-sidebar");
  info("Also enable values recording");
  await toggleJsTracerMenuItem(dbg, "#jstracer-menu-item-log-values");

  info("Enable the tracing");
  await toggleJsTracer(dbg.toolbox);

  const topLevelThreadActorID =
    dbg.toolbox.commands.targetCommand.targetFront.threadFront.actorID;
  info("Wait for tracing to be enabled");
  await waitForState(dbg, () => {
    return dbg.selectors.getIsThreadCurrentlyTracing(topLevelThreadActorID);
  });

  info("Trigger some code to record");
  invokeInTab("main");

  info("Wait for the call tree to appear in the tracer panel");
  const tracerTree = await waitForElementWithSelector(
    dbg,
    "#tracer-tab-panel .tree"
  );

  info("Wait for the expected traces to appear in the call tree");
  const traces = await waitFor(() => {
    const elements = tracerTree.querySelectorAll(".trace-line");
    if (elements.length == 3) {
      return elements;
    }
    return false;
  });
  tracerTree.ownerGlobal.focus();
  ok(traces[0].textContent.startsWith("λ main"));
  ok(traces[1].textContent.startsWith("λ foo"));
  ok(traces[2].textContent.startsWith("λ foo"));

  info("Select the trace for the first call to `foo`");
  EventUtils.synthesizeMouse(traces[1], 0, 0, {}, dbg.win);

  info("Wait for the trace location to be selected");
  await waitForSelectedLocation(dbg, 0, 69);

  const focusedTrace = tracerTree.querySelector(
    ".tree-node.focused .trace-line"
  );
  is(focusedTrace, traces[1], "The clicked trace is now focused");

  {
    const { element: popupEl, tokenEl } = await tryHovering(
      dbg,
      1,
      65,
      "previewPopup"
    );
    info("Wait for the preview popup to be populated");
    const objectPreview = await waitFor(() =>
      popupEl.querySelector(".node:nth-child(2)")
    );
    is(objectPreview.textContent, "a: 1");
    await closePreviewForToken(dbg, tokenEl, "previewPopup");
  }

  info("Select the trace for the second call to `foo`");
  EventUtils.synthesizeMouse(traces[2], 0, 0, {}, dbg.win);

  {
    const { element: popupEl, tokenEl } = await tryHovering(
      dbg,
      1,
      65,
      "previewPopup"
    );
    info("Wait for the preview popup to be populated");
    const objectPreview = await waitFor(() =>
      popupEl.querySelector(".node:nth-child(2)")
    );
    is(objectPreview.textContent, "b: 2");
    await closePreviewForToken(dbg, tokenEl, "previewPopup");
  }

  // Test Disabling tracing
  info("Disable the tracing");
  await toggleJsTracer(dbg.toolbox);
  info("Wait for tracing to be disabled");
  await waitForState(dbg, () => {
    return !dbg.selectors.getIsThreadCurrentlyTracing(topLevelThreadActorID);
  });

  info("Reset back to the default value");
  await toggleJsTracerMenuItem(dbg, "#jstracer-menu-item-console");
  await toggleJsTracerMenuItem(dbg, "#jstracer-menu-item-log-values");
});
