/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that after selecting a different stack frame, resuming reselects
 * the topmost stackframe, loads the right source in the editor pane and
 * highlights the proper line.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

var gTab, gPanel, gDebugger;
var gEditor, gSources, gFrames, gClassicFrames, gToolbar;

function test() {
  let options = {
    source: EXAMPLE_URL + "code_script-switching-01.js",
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gFrames = gDebugger.DebuggerView.StackFrames;
    gClassicFrames = gDebugger.DebuggerView.StackFramesClassicList;
    gToolbar = gDebugger.DebuggerView.Toolbar;

    waitForSourceAndCaretAndScopes(gPanel, "-02.js", 1).then(performTest);
    callInTab(gTab, "firstCall");
  });
}

function performTest() {
  return Task.spawn(function* () {
    yield selectBottomFrame();
    testBottomFrame(4);

    yield performStep("StepOver");
    testTopFrame(1);

    yield selectBottomFrame();
    testBottomFrame(4);

    yield performStep("StepIn");
    testTopFrame(1);

    yield selectBottomFrame();
    testBottomFrame(4);

    yield performStep("StepOut");
    testTopFrame(1);

    yield resumeDebuggerThenCloseAndFinish(gPanel);
  });

  function selectBottomFrame() {
    let shown = waitForSourceShown(gPanel, "-01.js");
    gClassicFrames.selectedIndex = gClassicFrames.itemCount - 1;
    return shown;
  }

  function testBottomFrame(debugLocation) {
    is(gFrames.selectedIndex, 0,
      "Oldest frame should be selected after click.");
    is(gClassicFrames.selectedIndex, gFrames.itemCount - 1,
      "Oldest frame should also be selected in the mirrored view.");
    is(gSources.selectedIndex, 0,
      "The first source is now selected in the widget.");
    is(gEditor.getText().search(/firstCall/), 118,
      "The first source is displayed.");
    is(gEditor.getText().search(/debugger/), -1,
      "The second source is not displayed.");

    is(gEditor.getDebugLocation(), debugLocation,
      "Editor debugger location is correct.");
    ok(gEditor.hasLineClass(debugLocation, "debug-line"),
      "The debugged line is highlighted appropriately.");
  }

  function performStep(type) {
    let updated = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES);
    gToolbar["_on" + type + "Pressed"]();
    return updated.then(waitForTick);
  }

  function testTopFrame(frameIndex) {
    is(gFrames.selectedIndex, frameIndex,
      "Topmost frame should be selected after click.");
    is(gClassicFrames.selectedIndex, gFrames.itemCount - frameIndex - 1,
      "Topmost frame should also be selected in the mirrored view.");
    is(gSources.selectedIndex, 1,
      "The second source is now selected in the widget.");
    is(gEditor.getText().search(/firstCall/), -1,
      "The second source is displayed.");
    is(gEditor.getText().search(/debugger/), 166,
      "The first source is not displayed.");
  }
}

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
  gSources = null;
  gFrames = null;
  gClassicFrames = null;
  gToolbar = null;
});
