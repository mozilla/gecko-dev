/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if pausing and resuming in the main loop works properly.
 */

const TAB_URL = EXAMPLE_URL + "doc_pause-exceptions.html";

let gTab, gPanel, gDebugger;
let gResumeButton, gResumeKey, gFrames;

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gResumeButton = gDebugger.document.getElementById("resume");
    gResumeKey = gDebugger.document.getElementById("resumeKey");
    gFrames = gDebugger.DebuggerView.StackFrames;

    testPause();
  });
}

function testPause() {
  is(gDebugger.gThreadClient.paused, false,
    "Should be running after starting the test.");

  is(gResumeButton.getAttribute("tooltiptext"),
     gDebugger.L10N.getFormatStr("pauseButtonTooltip",
      gDebugger.ShortcutUtils.prettifyShortcut(gResumeKey)),
    "Button tooltip should be 'pause' when running.");

  gDebugger.gThreadClient.addOneTimeListener("paused", () => {
    is(gDebugger.gThreadClient.paused, true,
      "Should be paused after an interrupt request.");

    is(gResumeButton.getAttribute("tooltiptext"),
       gDebugger.L10N.getFormatStr("resumeButtonTooltip",
        gDebugger.ShortcutUtils.prettifyShortcut(gResumeKey)),
      "Button tooltip should be 'resume' when paused.");

    is(gFrames.itemCount, 0,
      "Should have no frames when paused in the main loop.");

    testResume();
  });

  EventUtils.sendMouseEvent({ type: "mousedown" }, gResumeButton, gDebugger);
}

function testResume() {
  gDebugger.gThreadClient.addOneTimeListener("resumed", () => {
    is(gDebugger.gThreadClient.paused, false,
      "Should be paused after an interrupt request.");

    is(gResumeButton.getAttribute("tooltiptext"),
       gDebugger.L10N.getFormatStr("pauseButtonTooltip",
        gDebugger.ShortcutUtils.prettifyShortcut(gResumeKey)),
      "Button tooltip should be pause when running.");

    closeDebuggerAndFinish(gPanel);
  });

  EventUtils.sendMouseEvent({ type: "mousedown" }, gResumeButton, gDebugger);
}

registerCleanupFunction(function() {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gResumeButton = null;
  gResumeKey = null;
  gFrames = null;
});
