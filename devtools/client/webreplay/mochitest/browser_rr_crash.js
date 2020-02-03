/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test web replay crash recovery.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_basic.html", {
    waitForRecording: true,
  });

  const { threadFront, toolbox } = dbg;
  await addBreakpoint(dbg, "doc_rr_basic.html", 21);

  threadFront.replayTriggerCrash("runToPoint");
  await rewindToLine(dbg, 21);

  // FIXME disabling for now due to weird socket related crashes when running locally.
  /*
  threadFront.replayTriggerCrash("fork");
  await rewindToLine(dbg, 21);
  */

  ok(true, "Recovered from crashes");

  await shutdownDebugger(dbg);
});
