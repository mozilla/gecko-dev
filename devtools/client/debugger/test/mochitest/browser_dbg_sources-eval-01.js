/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure eval scripts appear in the source list
 */

const TAB_URL = EXAMPLE_URL + "doc_script-eval.html";

function test() {
  let gTab, gPanel, gDebugger;
  let gSources, gBreakpoints;

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

    return Task.spawn(function* () {
      is(gSources.values.length, 1, "Should have 1 source");

      let newSource = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.NEW_SOURCE);
      callInTab(gTab, "evalSource");
      yield newSource;

      is(gSources.values.length, 2, "Should have 2 sources");

      let item = gSources.getItemForAttachment(e => e.label.indexOf("> eval") !== -1);
      ok(item, "Source label is incorrect.");
      is(item.attachment.group, gDebugger.L10N.getStr("evalGroupLabel"),
         "Source group is incorrect");

      yield closeDebuggerAndFinish(gPanel);
    });
  });
}
