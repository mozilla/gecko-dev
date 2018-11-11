/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the global search results switch back and forth, and wrap around
 * when switching between them.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

var gTab, gPanel, gDebugger;
var gEditor, gSources, gSearchView, gSearchBox;

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
    gSearchView = gDebugger.DebuggerView.GlobalSearch;
    gSearchBox = gDebugger.DebuggerView.Filtering._searchbox;

    function firstSearch() {
      let deferred = promise.defer();

      is(gSearchView.itemCount, 0,
         "The global search pane shouldn't have any entries yet.");
      is(gSearchView.widget._parent.hidden, true,
         "The global search pane shouldn't be visible yet.");
      is(gSearchView._splitter.hidden, true,
         "The global search pane splitter shouldn't be visible yet.");

      gDebugger.once(gDebugger.EVENTS.GLOBAL_SEARCH_MATCH_FOUND, () => {
        // Some operations are synchronously dispatched on the main thread,
        // to avoid blocking UI, thus giving the impression of faster searching.
        executeSoon(() => {
          info("Current source url:\n" + getSelectedSourceURL(gSources));
          info("Debugger editor text:\n" + gEditor.getText());

          ok(isCaretPos(gPanel, 6),
             "The editor shouldn't have jumped to a matching line yet.");
          ok(getSelectedSourceURL(gSources).includes("-02.js"),
             "The current source shouldn't have changed after a global search.");
          is(gSources.visibleItems.length, 2,
             "Not all the sources are shown after the global search.");

          deferred.resolve();
        });
      });

      setText(gSearchBox, "!function");

      return deferred.promise;
    }

    function doFirstJump() {
      let deferred = promise.defer();

      waitForSourceAndCaret(gPanel, "-01.js", 4).then(() => {
        info("Current source url:\n" + getSelectedSourceURL(gSources));
        info("Debugger editor text:\n" + gEditor.getText());

        ok(getSelectedSourceURL(gSources).includes("-01.js"),
           "The currently shown source is incorrect (1).");
        is(gSources.visibleItems.length, 2,
           "Not all the sources are shown after the global search (1).");

        // The editor's selected text takes a tick to update.
        ok(isCaretPos(gPanel, 4, 9),
           "The editor didn't jump to the correct line (1).");
        is(gEditor.getSelection(), "function",
           "The editor didn't select the correct text (1).");

        deferred.resolve();
      });

      EventUtils.sendKey("DOWN", gDebugger);

      return deferred.promise;
    }

    function doSecondJump() {
      let deferred = promise.defer();

      waitForSourceAndCaret(gPanel, "-02.js", 4).then(() => {
        info("Current source url:\n" + getSelectedSourceURL(gSources));
        info("Debugger editor text:\n" + gEditor.getText());

        ok(getSelectedSourceURL(gSources).includes("-02.js"),
           "The currently shown source is incorrect (2).");
        is(gSources.visibleItems.length, 2,
           "Not all the sources are shown after the global search (2).");

        ok(isCaretPos(gPanel, 4, 9),
           "The editor didn't jump to the correct line (2).");
        is(gEditor.getSelection(), "function",
           "The editor didn't select the correct text (2).");

        deferred.resolve();
      });

      EventUtils.sendKey("DOWN", gDebugger);

      return deferred.promise;
    }

    function doWrapAroundJump() {
      let deferred = promise.defer();

      waitForSourceAndCaret(gPanel, "-01.js", 4).then(() => {
        info("Current source url:\n" + getSelectedSourceURL(gSources));
        info("Debugger editor text:\n" + gEditor.getText());

        ok(getSelectedSourceURL(gSources).includes("-01.js"),
           "The currently shown source is incorrect (3).");
        is(gSources.visibleItems.length, 2,
           "Not all the sources are shown after the global search (3).");

        // The editor's selected text takes a tick to update.
        ok(isCaretPos(gPanel, 4, 9),
           "The editor didn't jump to the correct line (3).");
        is(gEditor.getSelection(), "function",
           "The editor didn't select the correct text (3).");

        deferred.resolve();
      });

      EventUtils.sendKey("DOWN", gDebugger);
      EventUtils.sendKey("DOWN", gDebugger);

      return deferred.promise;
    }

    function doBackwardsWrapAroundJump() {
      let deferred = promise.defer();

      waitForSourceAndCaret(gPanel, "-02.js", 7).then(() => {
        info("Current source url:\n" + getSelectedSourceURL(gSources));
        info("Debugger editor text:\n" + gEditor.getText());

        ok(getSelectedSourceURL(gSources).includes("-02.js"),
           "The currently shown source is incorrect (4).");
        is(gSources.visibleItems.length, 2,
           "Not all the sources are shown after the global search (4).");

        // The editor's selected text takes a tick to update.
        ok(isCaretPos(gPanel, 7, 11),
           "The editor didn't jump to the correct line (4).");
        is(gEditor.getSelection(), "function",
           "The editor didn't select the correct text (4).");

        deferred.resolve();
      });

      EventUtils.sendKey("UP", gDebugger);

      return deferred.promise;
    }

    function testSearchTokenEmpty() {
      backspaceText(gSearchBox, 4);

      info("Current source url:\n" + getSelectedSourceURL(gSources));
      info("Debugger editor text:\n" + gEditor.getText());

      ok(getSelectedSourceURL(gSources).includes("-02.js"),
         "The currently shown source is incorrect (4).");
      is(gSources.visibleItems.length, 2,
         "Not all the sources are shown after the global search (4).");
      ok(isCaretPos(gPanel, 7, 11),
         "The editor didn't remain at the correct line (4).");
      is(gEditor.getSelection(), "",
         "The editor shouldn't keep the previous text selected (4).");

      is(gSearchView.itemCount, 0,
         "The global search pane shouldn't have any child nodes after clearing.");
      is(gSearchView.widget._parent.hidden, true,
         "The global search pane shouldn't be visible after clearing.");
      is(gSearchView._splitter.hidden, true,
         "The global search pane splitter shouldn't be visible after clearing.");
    }

    Task.spawn(function* () {
      yield waitForSourceAndCaretAndScopes(gPanel, "-02.js", 1);
      yield firstSearch();
      yield doFirstJump();
      yield doSecondJump();
      yield doWrapAroundJump();
      yield doBackwardsWrapAroundJump();
      yield testSearchTokenEmpty();
      resumeDebuggerThenCloseAndFinish(gPanel);
    });

    callInTab(gTab, "firstCall");
  });
}
