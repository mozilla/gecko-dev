/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test that workers function when recording/replaying. For now workers can't
// be inspected, so we're making sure that nothing crashes.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_worker.html", {
    waitForRecording: true,
  });

  await addBreakpoint(dbg, "doc_rr_worker.html", 15);
  await rewindToLine(dbg, 15);

  await shutdownDebugger(dbg);
});
