/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that the editor keeps proper scroll position per document
// while also moving to the correct location upon pause/breakpoint selection

"use strict";

// This test runs too slowly on linux debug. I'd like to figure out
// which is the slowest part of this and make it run faster, but to
// fix a frequent failure allow a longer timeout.
requestLongerTimeout(2);

/**
 * Test some simple usecases where the editor should scroll to the paused location
 * and remember the previously scrolled location when switching between two distinct sources.
 */
add_task(async function testScrollingOnPauseAndSourceSwitching() {
  const dbg = await initDebugger(
    "doc-editor-scroll.html",
    "scroll.js",
    "long.js"
  );

  // Set the initial breakpoint.
  await selectSource(dbg, "scroll.js");
  await addBreakpoint(dbg, "scroll.js", 26);

  info("Open long file, scroll down to line below the fold");
  await selectSource(dbg, "long.js");

  await scrollEditorIntoView(dbg, 25, 0);
  ok(isScrolledPositionVisible(dbg, 25), "Scroll position is visible");

  info("Ensure vertical scroll is the same after switching documents");
  let onScrolled = waitForScrolling(dbg);
  await selectSource(dbg, "scroll.js");
  // Wait for any codemirror editor scroll that can happen
  await onScrolled;
  ok(isScrolledPositionVisible(dbg, 1), "Scrolled to the top of the editor");

  onScrolled = waitForScrolling(dbg);
  await selectSource(dbg, "long.js");
  await onScrolled;
  ok(isScrolledPositionVisible(dbg, 25), "Scroll position is visible");

  info("Trigger a pause, click on a frame, ensure the right line is selected");
  onScrolled = waitForScrolling(dbg);
  invokeInTab("line26");
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
  ok(isScrolledPositionVisible(dbg, 25), "Scroll position is visible");

  info("Open new source, ensure it's at 0 scroll");
  onScrolled = waitForScrolling(dbg);
  await selectSource(dbg, "frames.js");
  await onScrolled;
  ok(isScrolledPositionVisible(dbg, 1), "Scrolled to the top of the editor");
});

/**
 * Some extensive test against Editor's isPositionVisible implementation.
 *
 * Assert precisely which lines are considered visible or hidden by this method,
 * while ensuring that the editor scrolls to the pause location in some edgecases.
 * For example, when the line is partially visible at the end of the viewport.
 */
add_task(async function testIsPositionVisible() {
  // Ensure having the default fixed height, as it can impact the number of displayed lines
  await pushPref("devtools.toolbox.footer.height", 250);

  const dbg = await initDebugger(
    "doc-editor-scroll.html",
    "scroll.js",
    "long.js"
  );

  await selectSource(dbg, "scroll.js");
  const editor = getCMEditor(dbg);

  function getFirstLine() {
    const { x, y } = editor.codeMirror.dom.getBoundingClientRect();
    // Add a pixel as we may be on the edge of the previous line which is hidden
    const pos = editor.codeMirror.posAtCoords({ x, y: y + 1 });
    return editor.codeMirror.state.doc.lineAt(pos).number;
  }
  function getLastLine() {
    const { x, y, height } = editor.codeMirror.dom.getBoundingClientRect();
    const pos = editor.codeMirror.posAtCoords({ x, y: y + height });
    return editor.codeMirror.state.doc.lineAt(pos).number;
  }
  const lastLine = getLastLine();

  is(
    lastLine,
    11,
    "The last line is the 11th. (it may change if you resize the default browser window height)"
  );
  ok(isScrolledPositionVisible(dbg, 1), "First line is visible");
  ok(
    isScrolledPositionVisible(dbg, lastLine - 1),
    "The line before the last one is visible"
  );
  ok(
    !isScrolledPositionVisible(dbg, lastLine),
    "Last line is partially visible and considered hidden"
  );
  ok(
    !isScrolledPositionVisible(dbg, lastLine + 1),
    "The line after the last is hidden"
  );

  info(
    "Set a breakpoint and pause on the last fully visible line, it should not scroll"
  );
  await addBreakpoint(dbg, "scroll.js", lastLine - 1);
  invokeInTab("line" + (lastLine - 1));
  await waitForPaused(dbg);

  ok(
    !isScrolledPositionVisible(dbg, lastLine),
    "Last line, which is partially visible, is still hidden"
  );

  await resume(dbg);

  info(
    "Set a breakpoint on the last partially visibible line, it should scroll that line in the middle of the viewport"
  );
  await addBreakpoint(dbg, "scroll.js", lastLine);
  invokeInTab("line" + lastLine);
  await waitForPaused(dbg);

  const newLastLine = getLastLine();
  is(newLastLine, 16, "The new last line is the 16th");
  ok(
    !isScrolledPositionVisible(dbg, newLastLine),
    "The new Last line is still partially visible and considered hidden"
  );
  ok(
    isScrolledPositionVisible(dbg, newLastLine - 1),
    "The line before is reported as visible"
  );
  const firstLine = getFirstLine();
  is(firstLine, 6);
  ok(
    isScrolledPositionVisible(dbg, firstLine),
    "The new first line is visible"
  );
  ok(
    !isScrolledPositionVisible(dbg, firstLine - 1),
    "The line before the new first line is hidden"
  );

  await resume(dbg);

  info(
    "Set a breakpoint far from the current position, it should also scroll and display the paused line in the middle of the viewport"
  );
  await addBreakpoint(dbg, "scroll.js", 50);
  invokeInTab("line50");
  await waitForPaused(dbg);

  const newLastLine2 = getLastLine();
  is(newLastLine2, 55);
  ok(
    !isScrolledPositionVisible(dbg, newLastLine2),
    "The new last line is partially visible and considered as hidden"
  );
  ok(
    isScrolledPositionVisible(dbg, newLastLine2 - 1),
    "The line before is visible"
  );
  const firstLine2 = getFirstLine();
  is(firstLine2, 45);
  ok(
    isScrolledPositionVisible(dbg, firstLine2),
    "The new first line is visible"
  );
  ok(
    !isScrolledPositionVisible(dbg, firstLine2 - 1),
    "The line before the new first line is hidden"
  );

  await resume(dbg);
});
