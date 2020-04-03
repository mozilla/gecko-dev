/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Make sure that previews work as expected when using source maps.

// Test previews when switching between frames and stepping.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_preview_bundle.html", {
    waitForRecording: true,
  });

  await waitForSources(dbg, "bundle_input.js");

  await addBreakpoint(dbg, "bundle_input.js", 16);
  await rewindToLine(dbg, 16);

  await waitUntil(() => dbg.selectors.getSelectedInlinePreviews());

  await checkInlinePreview(dbg, [5, 6]);

  await waitForInstantStep(dbg, "stepOver");
  await stepOverToLine(dbg, 17);

  await checkInlinePreview(dbg, [5, 6, "new"]);

  await waitForInstantStep(dbg, "reverseStepOver");
  await reverseStepOverToLine(dbg, 16);

  await checkInlinePreview(dbg, [5, 6]);

  await shutdownDebugger(dbg);
});
