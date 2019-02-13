/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the highlighter element picker still works through iframe
// navigations.

const TEST_URI = "data:text/html;charset=utf-8," +
  "<p>bug 699308 - test iframe navigation</p>" +
  "<iframe src='data:text/html;charset=utf-8,hello world'></iframe>";

add_task(function* () {
  let { inspector, toolbox } = yield openInspectorForURL(TEST_URI);
  let iframe = getNode("iframe");

  info("Starting element picker.");
  yield toolbox.highlighterUtils.startPicker();

  info("Waiting for highlighter to activate.");
  let highlighterShowing = toolbox.once("highlighter-ready");
  executeInContent("Test:SynthesizeMouse", {
    options: {type: "mousemove"},
    x: 1,
    y: 1
  }, {node: content.document.body}, false);
  yield highlighterShowing;

  let isVisible = yield isHighlighting(toolbox);
  ok(isVisible, "Inspector is highlighting.");

  yield reloadFrame();
  info("Frame reloaded. Reloading again.");

  yield reloadFrame();
  info("Frame reloaded twice.");

  isVisible = yield isHighlighting(toolbox);
  ok(isVisible, "Inspector is highlighting after iframe nav.");

  info("Stopping element picker.");
  yield toolbox.highlighterUtils.stopPicker();

  function reloadFrame() {
    info("Reloading frame.");
    let frameLoaded = once(iframe, "load");
    iframe.contentWindow.location.reload();
    return frameLoaded;
  }
});
