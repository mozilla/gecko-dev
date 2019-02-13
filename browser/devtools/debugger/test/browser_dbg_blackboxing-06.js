/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that clicking the black box checkbox when paused doesn't re-select the
 * currently paused frame's source.
 */

const TAB_URL = EXAMPLE_URL + "doc_blackboxing.html";

let gTab, gPanel, gDebugger;
let gSources;

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;

    waitForSourceAndCaretAndScopes(gPanel, ".html", 21)
      .then(testBlackBox)
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    callInTab(gTab, "runTest");
  });
}

function testBlackBox() {
  const selectedActor = gSources.selectedValue;

  let finished = waitForSourceShown(gPanel, "blackboxme.js").then(() => {
    const newSelectedActor = gSources.selectedValue;
    isnot(selectedActor, newSelectedActor,
      "Should not have the same url selected.");

    return toggleBlackBoxing(gPanel).then(() => {
      is(gSources.selectedValue, newSelectedActor,
        "The selected source did not change.");
    });
  });

  gSources.selectedIndex = 0;
  return finished;
}

registerCleanupFunction(function() {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gSources = null;
});
