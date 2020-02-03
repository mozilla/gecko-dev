/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test console evaluation features in web replay.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_basic.html", {
    waitForRecording: true,
  });

  const { toolbox } = dbg;
  const console = await toolbox.selectTool("webconsole");
  const { hud } = console;

  BrowserTest.execute(hud, "number");
  await waitForMessage(hud, "10");

  BrowserTest.execute(hud, "window.updateNumber");
  await waitForMessage(hud, "function updateNumber");

  ok(true, "Evaluations worked");

  await shutdownDebugger(dbg);
});
