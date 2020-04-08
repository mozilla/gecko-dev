/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test that GC works.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_gc.html", {
    waitForRecording: true,
  });

  await addBreakpoint(dbg, "doc_rr_gc.html", 23);
  await rewindToLine(dbg, 23);

  ok(true, "Page GC'ed and nothing crashed");

  await shutdownDebugger(dbg);
});
