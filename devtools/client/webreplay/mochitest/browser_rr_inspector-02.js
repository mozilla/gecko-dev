/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/shared-head.js",
  this
);

// Test that the element highlighter works when paused and replaying.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_inspector_basic.html", {
    waitForRecording: true,
  });
  const { toolbox } = dbg;

  await addBreakpoint(dbg, "doc_inspector_basic.html", 9);
  await rewindToLine(dbg, 9);

  const { testActor, inspector } = await openInspector();

  info("Waiting for element picker to become active.");
  toolbox.win.focus();

  await toolbox.nodePicker.start();

  info("Moving mouse over div.");
  await moveMouseOver("#maindiv", 1, 1);

  // Checks in isNodeCorrectlyHighlighted are off for an unknown reason, even
  // though the highlighting appears correctly in the UI.
  info("Performing checks");
  await testActor.isNodeCorrectlyHighlighted("#maindiv", is);

  // EventUtils.synthesizeMouse() can't figure out the exact position on the
  // screen of elements in iframes when replaying. Pass in some suitable values
  // so that the right position is clicked. This is pretty cheesy.
  await moveMouseOver(["#iframe", "#iframediv"], 10, 48);
  await testActor.isNodeCorrectlyHighlighted(["#iframe", "#iframediv"], is);

  mouseClick(["#iframe", "#iframediv"], 10, 48);
  await waitUntil(() => {
    const container = inspector.markup.getSelectedContainer();
    return container && container.editor.textEditor &&
      container.editor.textEditor.textNode.state.value == "IFRAME";
  });

  await shutdownDebugger(dbg);

  function moveMouseOver(selector, x, y) {
    info("Waiting for element " + selector + " to be highlighted");
    testActor.synthesizeMouse({
      selector,
      x,
      y,
      options: { type: "mousemove" },
    });
    return toolbox.nodePicker.once("picker-node-hovered");
  }

  function mouseClick(selector, x, y) {
    testActor.synthesizeMouse({
      selector,
      x,
      y,
      options: {},
    });
  }
});
