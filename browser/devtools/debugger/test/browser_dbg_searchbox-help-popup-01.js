/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the searchbox popup is displayed when focusing the searchbox,
 * and hidden when the user starts typing.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

let gTab, gDebuggee, gPanel, gDebugger;
let gSearchBox, gSearchBoxPanel;

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gSearchBox = gDebugger.DebuggerView.Filtering._searchbox;
    gSearchBoxPanel = gDebugger.DebuggerView.Filtering._searchboxHelpPanel;

    waitForSourceAndCaretAndScopes(gPanel, "-02.js", 1)
      .then(showPopup)
      .then(hidePopup)
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    gDebuggee.firstCall();
  });
}

function showPopup() {
  is(gSearchBoxPanel.state, "closed",
    "The search box panel shouldn't be visible yet.");

  let finished = once(gSearchBoxPanel, "popupshown");
  EventUtils.sendMouseEvent({ type: "click" }, gSearchBox, gDebugger);
  return finished;
}

function hidePopup() {
  is(gSearchBoxPanel.state, "open",
    "The search box panel should be visible after searching started.");

  let finished = once(gSearchBoxPanel, "popuphidden");
  setText(gSearchBox, "#");
  return finished;
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gSearchBox = null;
  gSearchBoxPanel = null;
});
