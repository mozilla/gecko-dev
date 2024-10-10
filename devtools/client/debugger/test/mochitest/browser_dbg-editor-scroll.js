/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that the editor keeps proper scroll position per document
// while also moving to the correct location upon pause/breakpoint selection

"use strict";

requestLongerTimeout(2);

add_task(async function () {
  // This test runs too slowly on linux debug. I'd like to figure out
  // which is the slowest part of this and make it run faster, but to
  // fix a frequent failure allow a longer timeout.
  const dbg = await initDebugger("doc-editor-scroll.html");

  // Set the initial breakpoint.
  await selectSource(dbg, "simple1.js");
  await addBreakpoint(dbg, "simple1.js", 26);

  info("Open long file, scroll down to line below the fold");
  await selectSource(dbg, "long.js");

  await scrollEditorIntoView(dbg, 24, 0);
  ok(isScrolledPositionVisible(dbg, 24), "Scroll position is visible");

  info("Ensure vertical scroll is the same after switching documents");
  let onScrolled = waitForScrolling(dbg);
  await selectSource(dbg, "simple1.js");
  // Wait for any codemirror editor scroll that can happen
  await onScrolled;
  ok(isScrolledPositionVisible(dbg, 0), "Scrolled to the top of the editor");

  onScrolled = waitForScrolling(dbg);
  await selectSource(dbg, "long.js");
  await onScrolled;
  ok(isScrolledPositionVisible(dbg, 24), "Scroll position is visible");

  info("Trigger a pause, click on a frame, ensure the right line is selected");
  onScrolled = waitForScrolling(dbg);
  invokeInTab("doNamedEval");
  await waitForPaused(dbg);
  await onScrolled;

  ok(
    isScrolledPositionVisible(dbg, 26),
    "Frame scrolled down to correct location"
  );

  info("Navigating while paused, goes to the correct location");
  onScrolled = waitForScrolling(dbg);
  await selectSource(dbg, "long.js");
  await onScrolled;
  ok(isScrolledPositionVisible(dbg, 24), "Scroll position is visible");

  info("Open new source, ensure it's at 0 scroll");
  onScrolled = waitForScrolling(dbg);
  await selectSource(dbg, "frames.js");
  await onScrolled;
  ok(isScrolledPositionVisible(dbg, 0), "Scrolled to the top of the editor");
});
