/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that makes sure web console autocomplete happens in the user-selected
// stackframe from the js debugger.

"use strict";

// Import helpers for the new debugger
/* import-globals-from ../../../debugger/new/test/mochitest/helpers.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/debugger/new/test/mochitest/helpers.js",
  this);

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/mochitest/test-autocomplete-in-stackframe.html";

requestLongerTimeout(20);

add_task(async function() {
  // Run test with legacy JsTerm
  await pushPref("devtools.webconsole.jsterm.codeMirror", false);
  await performTests();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  const { jsterm } = await openNewTabAndConsole(TEST_URI);
  const {
    autocompletePopup: popup,
  } = jsterm;

  const target = await TargetFactory.forTab(gBrowser.selectedTab);
  const toolbox = gDevTools.getToolbox(target);

  const jstermComplete = value => setInputValueForAutocompletion(jsterm, value);

  // Test that document.title gives string methods. Native getters must execute.
  await jstermComplete("document.title.");

  const newItemsLabels = getPopupLabels(popup);
  ok(newItemsLabels.length > 0, "'document.title.' gave a list of suggestions");
  ok(newItemsLabels.includes("substr"), `results do contain "substr"`);
  ok(newItemsLabels.includes("toLowerCase"), `results do contain "toLowerCase"`);
  ok(newItemsLabels.includes("strike"), `results do contain "strike"`);

  // Test if 'foo' gives 'foo1' but not 'foo2' or 'foo3'
  await jstermComplete("foo");
  is(getPopupLabels(popup).join("-"), "foo1-foo1Obj",
    `"foo" gave the expected suggestions`);

  // Test if 'foo1Obj.' gives 'prop1' and 'prop2'
  await jstermComplete("foo1Obj.");
  checkJsTermCompletionValue(jsterm, "        prop1", "foo1Obj completion");
  is(getPopupLabels(popup).join("-"), "prop1-prop2",
    `"foo1Obj." gave the expected suggestions`);

  // Test if 'foo1Obj.prop2.' gives 'prop21'
  await jstermComplete("foo1Obj.prop2.");
  ok(getPopupLabels(popup).includes("prop21"),
    `"foo1Obj.prop2." gave the expected suggestions`);

  info("Opening Debugger");
  await openDebugger();
  const dbg = createDebuggerContext(toolbox);

  info("Waiting for pause");
  await pauseDebugger(dbg);
  const stackFrames = dbg.selectors.getCallStackFrames(dbg.getState());

  info("Opening Console again");
  await toolbox.selectTool("webconsole");

  // Test if 'foo' gives 'foo3' and 'foo1' but not 'foo2', since we are paused in
  // the `secondCall` function (called by `firstCall`, which we call in `pauseDebugger`).
  await jstermComplete("foo");
  is(getPopupLabels(popup).join("-"), "foo1-foo1Obj-foo3-foo3Obj",
    `"foo" gave the expected suggestions`);

  await openDebugger();

  // Select the frame for the `firstCall` function.
  await dbg.actions.selectFrame(stackFrames[1]);

  info("openConsole");
  await toolbox.selectTool("webconsole");

  // Test if 'foo' gives 'foo2' and 'foo1' but not 'foo3', since we are now in the
  // `firstCall` frame.
  await jstermComplete("foo");
  is(getPopupLabels(popup).join("-"), "foo1-foo1Obj-foo2-foo2Obj",
    `"foo" gave the expected suggestions`);

  // Test if 'foo2Obj.' gives 'prop1'
  await jstermComplete("foo2Obj.");
  ok(getPopupLabels(popup).includes("prop1"), `"foo2Obj." returns "prop1"`);

  // Test if 'foo2Obj.prop1.' gives 'prop11'
  await jstermComplete("foo2Obj.prop1.");
  ok(getPopupLabels(popup).includes("prop11"), `"foo2Obj.prop1" returns "prop11"`);

  // Test if 'foo2Obj.prop1.prop11.' gives suggestions for a string,i.e. 'length'
  await jstermComplete("foo2Obj.prop1.prop11.");
  ok(getPopupLabels(popup).includes("length"), `results do contain "length"`);

  // Test if 'foo2Obj[0].' throws no errors.
  await jstermComplete("foo2Obj[0].");
  is(getPopupLabels(popup).length, 0, "no items for foo2Obj[0]");
}

function getPopupLabels(popup) {
  return popup.getItems().map(item => item.label);
}

async function pauseDebugger(dbg) {
  info("Waiting for debugger to pause");
  ContentTask.spawn(gBrowser.selectedBrowser, {}, async function() {
    content.wrappedJSObject.firstCall();
  });
  await waitForPaused(dbg);
}
