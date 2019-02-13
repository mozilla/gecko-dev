/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that pressing ESC twice while in picker mode first stops the picker and
// then opens the split-console (see bug 988278).

const TEST_URL = "data:text/html;charset=utf8,<div></div>";

add_task(function*() {
  let {toolbox} = yield openInspectorForURL(TEST_URL);

  info("Start the element picker");
  yield toolbox.highlighterUtils.startPicker();

  info("Start using the picker by hovering over nodes");
  let onHover = toolbox.once("picker-node-hovered");
  executeInContent("Test:SynthesizeMouse", {
    options: {type: "mousemove"},
    center: true,
    selector: "div"
  }, null, false);
  yield onHover;

  info("Press escape and wait for the picker to stop");
  let onPickerStopped = toolbox.once("picker-stopped");
  executeInContent("Test:SynthesizeKey", {
    key: "VK_ESCAPE",
    options: {}
  }, null, false);
  yield onPickerStopped;

  info("Press escape again and wait for the split console to open");
  let onSplitConsole = toolbox.once("split-console");
  let onConsoleReady = toolbox.once("webconsole-ready");
  // The escape key is synthesized in the main process, which is where the focus
  // should be after the picker was stopped.
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  yield onSplitConsole;
  yield onConsoleReady;
  ok(toolbox.splitConsole, "The split console is shown.");

  // Hide the split console.
  yield toolbox.toggleSplitConsole();
});
