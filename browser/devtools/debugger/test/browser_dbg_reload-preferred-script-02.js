/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the preferred source is shown when a page is loaded and
 * the preferred source is specified after another source might have been shown.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";
const PREFERRED_URL = EXAMPLE_URL + "code_script-switching-02.js";

let gTab, gPanel, gDebugger;
let gSources;

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;

    waitForSourceShown(gPanel, PREFERRED_URL).then(finishTest);
    gSources.preferredSource = getSourceActor(gSources, PREFERRED_URL);
  });
}

function finishTest() {
  info("Currently preferred source: " + gSources.preferredValue);
  info("Currently selected source: " + gSources.selectedValue);

  is(getSourceURL(gSources, gSources.preferredValue), PREFERRED_URL,
    "The preferred source url wasn't set correctly.");
  is(getSourceURL(gSources, gSources.selectedValue), PREFERRED_URL,
    "The selected source isn't the correct one.");

  closeDebuggerAndFinish(gPanel);
}

registerCleanupFunction(function() {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gSources = null;
});
