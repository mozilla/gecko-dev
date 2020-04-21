/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

async function getMessageObjectInspector(hud, text) {
  const msg = await waitForMessage(hud, text);
  msg.scrollIntoView();

  const objectInspectors = [...msg.querySelectorAll(".tree")];
  ok(objectInspectors.length == 1, "got one object inspector");
  return objectInspectors[0];
}

function waitForInspectorNodeCount(oi, count) {
  return waitUntil(() => {
    const { length } = oi.querySelectorAll(".node");
    return length == count;
  });
}

async function expandInspectorArrow(oi, index, nodeCount) {
  oi.querySelectorAll(".arrow")[index].click();
  await waitForInspectorNodeCount(oi, nodeCount);
}

function waitForInspectorNodeValue(oi, text) {
  return waitUntil(() => {
    const nodes = oi.querySelectorAll(".node");
    for (const node of nodes) {
      if (node.textContent.includes(text)) {
        return true;
      }
    }
    return false;
  });
}

// Test expanding console objects that were logged by console messages,
// logpoints, and evaluations when the debugger is somewhere else.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_console.html", {
    waitForRecording: true,
  });

  const { toolbox } = dbg;
  const console = await toolbox.selectTool("webconsole");
  const { hud } = console;
  let oi;

  oi = await getMessageObjectInspector(hud, "Iteration 3");
  await waitForInspectorNodeCount(oi, 1);
  await expandInspectorArrow(oi, 0, 4);
  await expandInspectorArrow(oi, 1, 7);
  await waitForInspectorNodeValue(oi, "subobj: Object { subvalue: 9 }");

  oi = await getMessageObjectInspector(hud, "Iteration 5");
  await waitForInspectorNodeCount(oi, 1);
  await expandInspectorArrow(oi, 0, 4);
  await expandInspectorArrow(oi, 1, 7);
  await waitForInspectorNodeValue(oi, "subobj: Object { subvalue: 15 }");

  oi = await getMessageObjectInspector(hud, "Iteration 7");
  await waitForInspectorNodeCount(oi, 1);
  await expandInspectorArrow(oi, 0, 4);
  await expandInspectorArrow(oi, 1, 7);
  await waitForInspectorNodeValue(oi, "subobj: Object { subvalue: 21 }");

  await addBreakpoint(dbg, "doc_rr_console.html", 16, undefined, {
    logValue: `"Logpoint",iteration,object`,
  });

  oi = await getMessageObjectInspector(hud, "Logpoint 8");
  await waitForInspectorNodeCount(oi, 1);
  await expandInspectorArrow(oi, 0, 4);
  await expandInspectorArrow(oi, 1, 7);
  await waitForInspectorNodeValue(oi, "subobj: Object { subvalue: 24 }");

  await addBreakpoint(dbg, "doc_rr_console.html", 7);
  await rewindToLine(dbg, 7);

  BrowserTest.execute(hud, "object");
  oi = await getMessageObjectInspector(hud, "Object { obj: {…}, value: 0 }");
  await waitForInspectorNodeCount(oi, 1);

  await warpToMessage(hud, dbg, "Logpoint 2");

  await expandInspectorArrow(oi, 0, 4);
  await expandInspectorArrow(oi, 1, 7);
  await waitForInspectorNodeValue(oi, "subobj: Object { subvalue: 0 }");

  await shutdownDebugger(dbg);
});
