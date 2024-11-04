/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests the Javascript Tracing feature.

"use strict";
const jsCode = `
  function foo() { bar(42); };
  function bar(num) {plop(window)};
  function plop(win) {hey(win, false, null, undefined)};
  function hey(win) {}`;
const TEST_URL = `data:text/html,test-page<script>${encodeURIComponent(
  jsCode
)}</script>`;

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

  is(
    dbg.selectors.getSelectedPrimaryPaneTab(),
    "tracer",
    "The tracer sidebar is automatically shown on start"
  );

  invokeInTab("foo");

  info("Wait for the call tree to appear in the tracer panel");
  const tracerTree = await waitForElementWithSelector(
    dbg,
    "#tracer-tab-panel .tree"
  );

  info("Wait for the expected traces to appear in the call tree");
  // There is only one top level trace being displayed for `foo`  and one immediate children call to `bar`
  await waitFor(() => tracerTree.querySelectorAll(".trace-line").length == 2);

  const argumentSearchInput = findElementWithSelector(
    dbg,
    `#tracer-tab-panel .call-tree-container input`
  );
  async function checkSearchExpression(
    searchQuery,
    previewString,
    matchesCount
  ) {
    argumentSearchInput.value = "";
    type(dbg, searchQuery);

    await waitFor(() => {
      const argumentSearchValue = findElementWithSelector(
        dbg,
        `#tracer-tab-panel .call-tree-container .search-value`
      );
      return (
        argumentSearchValue.textContent ==
        `Searching for:${previewString} (${matchesCount} match(es))`
      );
    });
  }

  info("Search for function call whose arguments contains '42'");
  await checkSearchExpression("42", "42", 1);

  pressKey(dbg, "Enter");

  info("Wait for the matched trace to be focused");
  await waitFor(() =>
    tracerTree.querySelector(".tree-node.focused .trace-line")
  );

  let focusedTrace = tracerTree.querySelector(".tree-node.focused .trace-line");
  Assert.stringContains(focusedTrace.textContent, "λ bar");

  info("Search for some falsy values");
  await checkSearchExpression("false", "false", 1);
  await checkSearchExpression("null", "null", 1);
  await checkSearchExpression("undefined", "undefined", 1);

  info("Lookup for 'window' usages");
  await checkSearchExpression("window", `Window ${TEST_URL}`, 2);

  pressKey(dbg, "Enter");

  info("Wait for the matched trace to be focused");
  await waitFor(() =>
    tracerTree.querySelector(".tree-node.focused .trace-line")
  );

  focusedTrace = tracerTree.querySelector(".tree-node.focused .trace-line");
  Assert.stringContains(focusedTrace.textContent, "λ plop");

  info("Lookup for the next match");
  pressKey(dbg, "Enter");

  await waitFor(
    () =>
      tracerTree.querySelector(".tree-node.focused .trace-line") !=
      focusedTrace,
    "Wait for focusing a different line"
  );

  focusedTrace = tracerTree.querySelector(".tree-node.focused .trace-line");
  Assert.stringContains(focusedTrace.textContent, "λ hey");

  info("Get back to the previous match");
  pressKey(dbg, "ShiftEnter");

  await waitFor(
    () =>
      tracerTree.querySelector(".tree-node.focused .trace-line") !=
      focusedTrace,
    "Wait for focusing back the first match"
  );

  focusedTrace = tracerTree.querySelector(".tree-node.focused .trace-line");
  Assert.stringContains(focusedTrace.textContent, "λ plop");

  info("Now type a bogus expression");
  argumentSearchInput.value = "";
  type(dbg, "bogus");
  info("Wait for the exception to be displayed");
  const argumentSearchException = await waitFor(() =>
    findElementWithSelector(
      dbg,
      `#tracer-tab-panel .call-tree-container .search-exception`
    )
  );
  await waitFor(() => {
    return (
      argumentSearchException.textContent ==
      "ReferenceError: bogus is not defined"
    );
  });

  info("Disable values recording before switching to next test");
  await toggleJsTracerMenuItem(dbg, "#jstracer-menu-item-log-values");
});
