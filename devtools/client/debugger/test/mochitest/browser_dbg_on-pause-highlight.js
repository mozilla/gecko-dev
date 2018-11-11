/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that debugger's tab is highlighted when it is paused and not the
 * currently selected tool.
 */

const TAB_URL = EXAMPLE_URL + "doc_recursion-stack.html";

var gTab, gPanel, gDebugger;
var gToolbox, gToolboxTab;

function test() {
  let options = {
    source: TAB_URL,
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gToolbox = gPanel._toolbox;
    gToolboxTab = gToolbox.doc.getElementById("toolbox-tab-jsdebugger");

    testPause();
  });
}

function testPause() {
  is(gDebugger.gThreadClient.paused, false,
    "Should be running after starting test.");

  gDebugger.gThreadClient.addOneTimeListener("paused", () => {
    gToolbox.selectTool("webconsole").then(() => {
      ok(gToolboxTab.hasAttribute("highlighted") &&
         gToolboxTab.getAttribute("highlighted") == "true",
        "The highlighted class is present");
      ok(!gToolboxTab.hasAttribute("selected") ||
          gToolboxTab.getAttribute("selected") != "true",
        "The tab is not selected");
    }).then(() => gToolbox.selectTool("jsdebugger")).then(() => {
      ok(gToolboxTab.hasAttribute("highlighted") &&
         gToolboxTab.getAttribute("highlighted") == "true",
        "The highlighted class is present");
      ok(gToolboxTab.hasAttribute("selected") &&
         gToolboxTab.getAttribute("selected") == "true",
        "...and the tab is selected, so the glow will not be present.");
    }).then(testResume);
  });

  EventUtils.sendMouseEvent({ type: "mousedown" },
    gDebugger.document.getElementById("resume"),
    gDebugger);

  // Evaluate a script to fully pause the debugger
  once(gDebugger.gClient, "willInterrupt").then(() => {
    evalInTab(gTab, "1+1;");
  });
}

function testResume() {
  gDebugger.gThreadClient.addOneTimeListener("resumed", () => {
    gToolbox.selectTool("webconsole").then(() => {
      ok(!gToolboxTab.classList.contains("highlighted"),
        "The highlighted class is not present now after the resume");
      ok(!gToolboxTab.hasAttribute("selected") ||
          gToolboxTab.getAttribute("selected") != "true",
        "The tab is not selected");
    }).then(() => closeDebuggerAndFinish(gPanel));
  });

  EventUtils.sendMouseEvent({ type: "mousedown" },
    gDebugger.document.getElementById("resume"),
    gDebugger);
}

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gToolbox = null;
  gToolboxTab = null;
});
