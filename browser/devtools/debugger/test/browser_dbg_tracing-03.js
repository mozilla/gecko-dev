/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

SimpleTest.requestCompleteLog();

/**
 * Test that we can jump to function definitions by clicking on logs.
 */

const TAB_URL = EXAMPLE_URL + "doc_tracing-01.html";

let gTab, gPanel, gDebugger, gSources;

function test() {
  SpecialPowers.pushPrefEnv({'set': [["devtools.debugger.tracer", true]]}, () => {
    initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
      gTab = aTab;
      gPanel = aPanel;
      gDebugger = gPanel.panelWin;
      gSources = gDebugger.DebuggerView.Sources;

      waitForSourceShown(gPanel, "code_tracing-01.js")
        .then(() => startTracing(gPanel))
        .then(() => clickButton())
        .then(() => waitForClientEvents(aPanel, "traces"))
        .then(() => {
          // Switch away from the JS file so we can make sure that clicking on a
          // log will switch us back to the correct JS file.
          gSources.selectedValue = getSourceActor(gSources, TAB_URL);
          return ensureSourceIs(aPanel, getSourceActor(gSources, TAB_URL), true);
        })
        .then(() => {
          const finished = waitForSourceShown(gPanel, "code_tracing-01.js");
          clickTraceLog();
          return finished;
        })
        .then(testCorrectLine)
        .then(() => stopTracing(gPanel))
        .then(() => {
          const deferred = promise.defer();
          SpecialPowers.popPrefEnv(deferred.resolve);
          return deferred.promise;
        })
        .then(() => closeDebuggerAndFinish(gPanel))
        .then(null, aError => {
          ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
        });
    });
  });
}

function clickButton() {
  generateMouseClickInTab(gTab, "content.document.querySelector('button')");
}

function clickTraceLog() {
  filterTraces(gPanel, t => t.querySelector(".trace-name[value=main]"))[0].click();
}

function testCorrectLine() {
  is(gDebugger.DebuggerView.editor.getCursor().line, 18,
     "The editor should have the function definition site's line selected.");
}

registerCleanupFunction(function() {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gSources = null;
});
