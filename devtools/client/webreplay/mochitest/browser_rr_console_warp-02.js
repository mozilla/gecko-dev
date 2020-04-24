/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef, no-unused-vars */

"use strict";

async function checkPausedMessage(hud, text) {
  await waitForMessage(hud, text, ".paused");
}

// Test which message is the paused one after warping, stepping, and evaluating.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_logs.html", {
    waitForRecording: true,
  });

  const console = await getDebuggerSplitConsole(dbg);
  const hud = console.hud;

  // When warping to a message, it is the paused one.
  await warpToMessage(hud, dbg, "number: 2");
  await checkPausedMessage(hud, "number: 2");

  await stepOverToLine(dbg, 20);
  await checkPausedMessage(hud, "number: 2");

  // When stepping back we end up earlier than the console call, even though we're
  // paused at the same line. This isn't ideal.
  await reverseStepOverToLine(dbg, 19);
  await checkPausedMessage(hud, "number: 1");

  // When rewinding before the first message, that message is still considered
  // the paused one so that the remaining messages can be grayed out. This also
  // isn't ideal.
  await addBreakpoint(dbg, "doc_rr_logs.html", 16);
  await rewindToLine(dbg, 16);
  await checkPausedMessage(hud, "number: 1");

  await warpToMessage(hud, dbg, "number: 2");
  await checkPausedMessage(hud, "number: 2");
  await stepOverToLine(dbg, 20);

  BrowserTest.execute(hud, "1 << 5");
  await checkPausedMessage(hud, "32");

  await stepOverToLine(dbg, 21);
  BrowserTest.execute(hud, "1 << 7");
  await checkPausedMessage(hud, "128");

  await reverseStepOverToLine(dbg, 20);
  await checkPausedMessage(hud, "32");

  BrowserTest.execute(hud, "1 << 6");
  await checkPausedMessage(hud, "64");

  await shutdownDebugger(dbg);
});
