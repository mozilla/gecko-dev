/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/shared-head.js",
  this
);

function getContainerForNodeFront(nodeFront, { markup }) {
  return markup.getContainer(nodeFront);
}

async function focusSearchBoxUsingShortcut(panelWin) {
  info("Focusing search box");
  const searchBox = panelWin.document.getElementById("inspector-searchbox");
  const focused = once(searchBox, "focus");

  panelWin.focus();

  synthesizeKeyShortcut("CmdOrCtrl+F");

  await focused;
}

// Test basic inspector functionality in web replay: the inspector is able to
// show contents when paused according to the child's current position.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_inspector_basic.html", {
    waitForRecording: true,
    skipInterrupt: true,
  });

  const { testActor, inspector } = await openInspector();

  let nodeFront = await getNodeFront("#maindiv", inspector);
  ok(!nodeFront, "No node front while unpaused");

  await interrupt(dbg);

  nodeFront = await getNodeFront("#maindiv", inspector);
  await waitFor(
    () => inspector.markup && getContainerForNodeFront(nodeFront, inspector)
  );
  let container = getContainerForNodeFront(nodeFront, inspector);

  ok(
    container.editor.textEditor.textNode.state.value == "GOODBYE",
    "Correct late element text"
  );

  await addBreakpoint(dbg, "doc_inspector_basic.html", 9);
  await rewindToLine(dbg, 9);

  nodeFront = await getNodeFront("#maindiv", inspector);
  await waitFor(
    () => inspector.markup && getContainerForNodeFront(nodeFront, inspector)
  );
  container = getContainerForNodeFront(nodeFront, inspector);
  ok(
    container.editor.textEditor.textNode.state.value == "HELLO",
    "Correct early element text"
  );

  // Test searching.
  await dbg.toolbox.selectTool("inspector");
  await focusSearchBoxUsingShortcut(inspector.panelWin);

  for (const key of ["S", "T", "U", "F", "F", "VK_RETURN"]) {
    EventUtils.synthesizeKey(key, {}, inspector.panelWin);
  }

  await waitForTime(1000);

  await testActor.isNodeCorrectlyHighlighted("#div3", is);

  await shutdownDebugger(dbg);
});
