/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that stackframes are added when debugger is paused in eval calls.
 */

const TAB_URL = EXAMPLE_URL + "doc_recursion-stack.html";

function test() {
  let options = {
    source: TAB_URL,
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gFrames = gDebugger.DebuggerView.StackFrames;
    const gClassicFrames = gDebugger.DebuggerView.StackFramesClassicList;

    const performTest = Task.async(function* () {
      is(gDebugger.gThreadClient.state, "paused",
         "Should only be getting stack frames while paused.");
      is(gFrames.itemCount, 2,
         "Should have two frames.");
      is(gClassicFrames.itemCount, 2,
         "Should also have only two in the mirrored view.");

      is(gFrames.getItemAtIndex(0).attachment.title,
         "evalCall", "Oldest frame name should be correct.");
      is(gFrames.getItemAtIndex(0).attachment.url,
         TAB_URL, "Oldest frame url should be correct.");
      is(gClassicFrames.getItemAtIndex(0).attachment.depth,
         0, "Oldest frame name is mirrored correctly.");

      is(gFrames.getItemAtIndex(1).attachment.title,
         "(eval)", "Newest frame name should be correct.");
      is(gFrames.getItemAtIndex(1).attachment.url,
         "SCRIPT0", "Newest frame url should be correct.");
      is(gClassicFrames.getItemAtIndex(1).attachment.depth,
         1, "Newest frame name is mirrored correctly.");

      is(gFrames.selectedIndex, 1,
         "Newest frame should be selected by default.");
      is(gClassicFrames.selectedIndex, 0,
         "Newest frame should be selected by default in the mirrored view.");

      isnot(gFrames.selectedIndex, 0,
            "Oldest frame should not be selected.");
      isnot(gClassicFrames.selectedIndex, 1,
            "Oldest frame should not be selected in the mirrored view.");

      EventUtils.sendMouseEvent({ type: "mousedown" },
                                gFrames.getItemAtIndex(0).target,
                                gDebugger);

      isnot(gFrames.selectedIndex, 1,
            "Newest frame should not be selected after click.");
      isnot(gClassicFrames.selectedIndex, 0,
            "Newest frame in the mirrored view should not be selected.");

      is(gFrames.selectedIndex, 0,
         "Oldest frame should be selected after click.");
      is(gClassicFrames.selectedIndex, 1,
         "Oldest frame in the mirrored view should be selected.");

      EventUtils.sendMouseEvent({ type: "mousedown" },
                                gFrames.getItemAtIndex(1).target.querySelector(".dbg-stackframe-title"),
                                gDebugger);
      // Give the UI some time to update. For some reason if we don't
      // do this there is global window leakage. We are continually
      // cleaning up our tests so this will be refactored out at some
      // point.
      yield waitForTime(1);

      is(gFrames.selectedIndex, 1,
         "Newest frame should be selected after click inside the newest frame.");
      is(gClassicFrames.selectedIndex, 0,
         "Newest frame in the mirrored view should be selected.");

      isnot(gFrames.selectedIndex, 0,
            "Oldest frame should not be selected after click inside the newest frame.");
      isnot(gClassicFrames.selectedIndex, 1,
            "Oldest frame in the mirrored view should not be selected.");

      EventUtils.sendMouseEvent({ type: "mousedown" },
                                gFrames.getItemAtIndex(0).target.querySelector(".dbg-stackframe-details"),
                                gDebugger);
      // See comment above on the same statement.
      yield waitForTime(1);

      isnot(gFrames.selectedIndex, 1,
            "Newest frame should not be selected after click inside the oldest frame.");
      isnot(gClassicFrames.selectedIndex, 0,
            "Newest frame in the mirrored view should not be selected.");

      is(gFrames.selectedIndex, 0,
         "Oldest frame should be selected after click inside the oldest frame.");
      is(gClassicFrames.selectedIndex, 1,
         "Oldest frame in the mirrored view should be selected.");

      resumeDebuggerThenCloseAndFinish(gPanel);
    });

    Task.spawn(function* () {
      yield waitForCaretAndScopes(gPanel, 1);
      performTest();
    });

    callInTab(gTab, "evalCall");
  });
}
