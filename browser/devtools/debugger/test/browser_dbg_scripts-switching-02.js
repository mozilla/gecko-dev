/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that switching the displayed source in the UI works as advertised.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-02.html";

let gTab, gDebuggee, gPanel, gDebugger;
let gEditor, gSources;

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;

    waitForSourceAndCaretAndScopes(gPanel, "-02.js", 1)
      .then(testSourcesDisplay)
      .then(testSwitchPaused1)
      .then(testSwitchPaused2)
      .then(testSwitchRunning)
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    gDebuggee.firstCall();
  });
}

let gLabel1 = "code_script-switching-01.js";
let gLabel2 = "code_script-switching-02.js";
let gParams = "?foo=bar,baz|lol";

function testSourcesDisplay() {
  let deferred = promise.defer();

  is(gSources.itemCount, 2,
    "Found the expected number of sources.");

  ok(gSources.containsValue(EXAMPLE_URL + gLabel1),
    "First source url is incorrect.");
  ok(gSources.containsValue(EXAMPLE_URL + gLabel2 + gParams),
    "Second source url is incorrect.");

  ok(gSources.getItemForAttachment(e => e.label == gLabel1),
    "First source label is incorrect.");
  ok(gSources.getItemForAttachment(e => e.label == gLabel2),
    "Second source label is incorrect.");

  ok(gSources.selectedItem,
    "There should be a selected item in the sources pane.");
  is(gSources.selectedValue, EXAMPLE_URL + gLabel2 + gParams,
    "The selected value is the sources pane is incorrect.");

  is(gEditor.getText().search(/firstCall/), -1,
    "The first source is not displayed.");
  is(gEditor.getText().search(/debugger/), 172,
    "The second source is displayed.");

  ok(isCaretPos(gPanel, 1),
    "Editor caret location is correct.");

  // The editor's debug location takes a tick to update.
  executeSoon(() => {
    is(gEditor.getDebugLocation(), 0,
      "Editor debugger location is correct.");
    ok(gEditor.hasLineClass(0, "debug-line"),
      "The debugged line is highlighted appropriately.");

    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN).then(deferred.resolve);
    gSources.selectedItem = e => e.attachment.label == gLabel1;
  });

  return deferred.promise;
}

function testSwitchPaused1() {
  let deferred = promise.defer();

  ok(gSources.selectedItem,
    "There should be a selected item in the sources pane.");
  is(gSources.selectedValue, EXAMPLE_URL + gLabel1,
    "The selected value is the sources pane is incorrect.");

  is(gEditor.getText().search(/firstCall/), 118,
    "The first source is displayed.");
  is(gEditor.getText().search(/debugger/), -1,
    "The second source is not displayed.");

  // The editor's debug location takes a tick to update.
  executeSoon(() => {
    ok(isCaretPos(gPanel, 1),
      "Editor caret location is correct.");

    is(gEditor.getDebugLocation(), null,
      "Editor debugger location is correct.");
    ok(!gEditor.hasLineClass(5, "debug-line"),
      "The debugged line highlight was removed.");

    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN).then(deferred.resolve);
    gSources.selectedItem = e => e.attachment.label == gLabel2;
  });

  return deferred.promise;
}

function testSwitchPaused2() {
  let deferred = promise.defer();

  ok(gSources.selectedItem,
    "There should be a selected item in the sources pane.");
  is(gSources.selectedValue, EXAMPLE_URL + gLabel2 + gParams,
    "The selected value is the sources pane is incorrect.");

  is(gEditor.getText().search(/firstCall/), -1,
    "The first source is not displayed.");
  is(gEditor.getText().search(/debugger/), 172,
    "The second source is displayed.");

  // The editor's debug location takes a tick to update.
  executeSoon(() => {
    ok(isCaretPos(gPanel, 1),
      "Editor caret location is correct.");
    is(gEditor.getDebugLocation(), 0,
      "Editor debugger location is correct.");
    ok(gEditor.hasLineClass(0, "debug-line"),
      "The debugged line is highlighted appropriately.");

    // Step out three times.
    waitForThreadEvents(gPanel, "paused").then(() => {
      waitForThreadEvents(gPanel, "paused").then(() => {
        waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN).then(deferred.resolve);
        gDebugger.gThreadClient.stepOut();
      });
      gDebugger.gThreadClient.stepOut();
    });
    gDebugger.gThreadClient.stepOut();
  });

  return deferred.promise;
}

function testSwitchRunning() {
  let deferred = promise.defer();

  ok(gSources.selectedItem,
    "There should be a selected item in the sources pane.");
  is(gSources.selectedValue, EXAMPLE_URL + gLabel1,
    "The selected value is the sources pane is incorrect.");

  is(gEditor.getText().search(/firstCall/), 118,
    "The first source is displayed.");
  is(gEditor.getText().search(/debugger/), -1,
    "The second source is not displayed.");

  // The editor's debug location takes a tick to update.
  executeSoon(() => {
    ok(isCaretPos(gPanel, 1),
      "Editor caret location is correct.");
    is(gEditor.getDebugLocation(), 0,
      "Editor debugger location is correct.");
    ok(gEditor.hasLineClass(0, "debug-line"),
      "The debugged line is highlighted appropriately.");

    deferred.resolve();
  });

  return deferred.promise;
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
  gSources = null;
  gLabel1 = null;
  gLabel2 = null;
  gParams = null;
});
