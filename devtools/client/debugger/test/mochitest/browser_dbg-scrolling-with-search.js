/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that while doing a file search, if the debugger pauses
// the source should scroll correctly to the paused location.

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-scripts.html", "long.js", "simple1.js");

  const longSrc = findSource(dbg, "long.js");
  await selectSource(dbg, "long.js");
  await addBreakpoint(dbg, longSrc, 9);

  info("Searching should jump to the match");
  pressKey(dbg, "fileSearch");
  type(dbg, "candidate");

  await waitFor(
    () => getSearchSelection(dbg).text == "candidate",
    "Wait for actual selection in CodeMirror"
  );
  await waitForSelectedLocation(dbg, 50, 53);

  is(
    getSearchSelection(dbg).line,
    49,
    `The line of first check occurence in long.js is selected`
  );
  ok(
    isScrolledPositionVisible(dbg, 49),
    "The search selection line is visible"
  );

  info("Switch to a different source");
  await selectSource(dbg, "simple1.js");

  info("Trigger a pause which should switch back to long.js");
  invokeInTab("testModel");

  await waitForPaused(dbg, "long.js");
  await waitForScrolling(dbg);

  await assertPausedAtSourceAndLine(dbg, longSrc.id, 9);
  ok(isScrolledPositionVisible(dbg, 9), "The paused line is visible");
});
