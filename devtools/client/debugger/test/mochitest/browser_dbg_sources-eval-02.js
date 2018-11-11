/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure eval scripts with the sourceURL pragma are correctly
 * displayed
 */

const TAB_URL = EXAMPLE_URL + "doc_script-eval.html";

function test() {
  let gTab, gPanel, gDebugger;
  let gSources, gBreakpoints, gEditor;

  let options = {
    source: EXAMPLE_URL + "code_script-eval.js",
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;
    gBreakpoints = gDebugger.DebuggerController.Breakpoints;
    gEditor = gDebugger.DebuggerView.editor;
    const constants = gDebugger.require("./content/constants");
    const queries = gDebugger.require("./content/queries");
    const actions = bindActionCreators(gPanel);
    const getState = gDebugger.DebuggerController.getState;

    return Task.spawn(function* () {
      is(queries.getSourceCount(getState()), 1, "Should have 1 source");

      const newSource = waitForDispatch(gPanel, constants.ADD_SOURCE);
      callInTab(gTab, "evalSourceWithSourceURL");
      yield newSource;

      is(queries.getSourceCount(getState()), 2, "Should have 2 sources");

      const source = queries.getSourceByURL(getState(), EXAMPLE_URL + "bar.js");
      ok(source, "Source exists.");

      let shown = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN);
      actions.selectSource(source);
      yield shown;

      ok(gEditor.getText().indexOf("bar = function() {") === 0,
         "Correct source is shown");

      yield closeDebuggerAndFinish(gPanel);
    });
  });
}
