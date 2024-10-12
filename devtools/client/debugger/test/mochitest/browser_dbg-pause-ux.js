/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-scripts.html");

  // Make sure that we can set a breakpoint on a line out of the
  // viewport, and that pausing there scrolls the editor to it.
  const longSrc = findSource(dbg, "long.js");
  await selectSource(dbg, "long.js");
  await addBreakpoint(dbg, longSrc, 66);
  invokeInTab("testModel");
  await waitForPaused(dbg, "long.js");
  // Some spurious scroll may happen late related to text content *and* late fetching of symbols
  await waitForScrolling(dbg);

  ok(isScrolledPositionVisible(dbg, 66), "The paused line is visible");

  info("1. adding a breakpoint should not scroll the editor");
  await scrollEditorIntoView(dbg, 0, 0);
  await addBreakpoint(dbg, longSrc, 11);
  ok(isScrolledPositionVisible(dbg, 0), "scroll position");

  info("2. searching should jump to the match");
  pressKey(dbg, "fileSearch");
  type(dbg, "check");

  await waitFor(
    () => getSearchSelection(dbg).text == "check",
    "Wait for actual selection in CodeMirror"
  );
  is(
    getSearchSelection(dbg).line,
    26,
    `The line of first check occurence in long.js is selected (this is ${
      isCm6Enabled ? "one" : "zero"
    }-based)`
  );
  // The column is the end of "check", so after 'k'
  is(
    getSearchSelection(dbg).column,
    51,
    "The column of first check occurence in long.js is selected (this is zero-based)"
  );

  ok(
    !isScrolledPositionVisible(dbg, 66),
    "The paused line is no longer visible"
  );
  ok(
    isScrolledPositionVisible(dbg, 26),
    "The line with the text match is now visible"
  );
});
