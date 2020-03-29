/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test previews when switching between frames and stepping.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_preview.html", {
    waitForRecording: true,
  });

  await addBreakpoint(dbg, "doc_rr_preview.html", 17);
  await rewindToLine(dbg, 17);

  await waitUntil(() => dbg.selectors.getSelectedInlinePreviews());

  await toggleNode(dbg, "barobj");
  await findNode(dbg, "barprop1");
  await findNode(dbg, "barprop2");
  ok(findNodeValue(dbg, "barprop1") == "2", "correct early property text");

  const frames = dbg.selectors.getFrames(dbg.selectors.getCurrentThread());
  await dbg.actions.selectFrame(getThreadContext(dbg), frames[1]);

  await toggleNode(dbg, "fooobj");
  await findNode(dbg, "fooprop1");
  await findNode(dbg, "fooprop2");

  await dbg.actions.selectFrame(getThreadContext(dbg), frames[0]);

  await stepOverToLine(dbg, 18);

  await findNode(dbg, "barprop1");
  ok(findNodeValue(dbg, "barprop1") == `"updated"`, "correct late property text");

  await shutdownDebugger(dbg);
});
