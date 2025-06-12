/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/*
 * Tests that log points are correctly logged to the console
 */

"use strict";

add_task(async function () {
  Services.prefs.setBoolPref("devtools.toolbox.splitconsole.open", true);
  const dbg = await initDebugger(
    "doc-script-switching.html",
    "script-switching-01.js"
  );

  const source = findSource(dbg, "script-switching-01.js");
  await selectSource(dbg, "script-switching-01.js");

  await getDebuggerSplitConsole(dbg);

  info(
    `Add a first log breakpoint with no argument, which will log "display name", i.e. firstCall`
  );
  await altClickElement(dbg, "gutterElement", 7);
  await waitForBreakpoint(dbg, "script-switching-01.js", 7);

  info("Add another log breakpoint with multiple arguments");
  await dbg.actions.addBreakpoint(createLocation({ line: 8, source }), {
    logValue: "'a', 'b', 'c', firstCall",
  });

  invokeInTab("firstCall");
  await waitForPaused(dbg);

  info("Wait for the two log breakpoints");
  await hasConsoleMessage(dbg, "firstCall");
  await hasConsoleMessage(dbg, "a b c");

  const { link, value } = await findConsoleMessage(dbg, "a b c");
  is(link, "script-switching-01.js:8:3", "logs should have the relevant link");
  is(value, "a b c \nfunction firstCall()", "logs should have multiple values");
  await removeBreakpoint(dbg, source.id, 7);
  await removeBreakpoint(dbg, source.id, 8);

  await resume(dbg);

  info(
    "Now set a log point calling a method with a debugger statement and a breakpoint, it shouldn't pause on the log point"
  );
  await addBreakpoint(dbg, "script-switching-01.js", 8);
  await addBreakpoint(dbg, "script-switching-01.js", 7);
  await dbg.actions.addBreakpoint(createLocation({ line: 7, source }), {
    logValue: "'second call', secondCall()",
  });

  invokeInTab("firstCall");
  await waitForPaused(dbg);
  // We aren't pausing on secondCall's debugger statement,
  // called by the condition, but only on the breakpoint we set on firstCall, line 8
  await assertPausedAtSourceAndLine(dbg, source.id, 8);

  // The log point is visible, even if it had a debugger statement in it.
  await hasConsoleMessage(dbg, "second call 44");
  await removeBreakpoint(dbg, source.id, 7);
  await removeBreakpoint(dbg, source.id, 8);

  await resume(dbg);
  // Resume a second time as we are hittin the debugger statement as firstCall calls secondCall
  await resume(dbg);

  info(
    "Set a log point throwing an exception and ensure the exception is displayed"
  );
  await dbg.actions.addBreakpoint(createLocation({ line: 7, source }), {
    logValue: "jsWithError(",
  });
  invokeInTab("firstCall");
  await waitForPaused(dbg);
  // Exceptions in conditional breakpoint would not trigger a pause,
  // So we end up pausing on the debugger statement in the other script.
  await assertPausedAtSourceAndLine(
    dbg,
    findSource(dbg, "script-switching-02.js").id,
    6
  );

  // But verify that the exception message is visible even if we didn't pause.
  // As the logValue is evaled within an array `[${logValue}]`,
  // the exception message is a bit cryptic...
  await hasConsoleMessage(dbg, "expected expression, got ']'");

  await dbg.actions.removeAllBreakpoints();

  await resume(dbg);
  info("About to set log point");

  await selectSource(dbg, "script-switching-01.js");

  await setLogPoint(dbg, 8, "'stacktrace test'", true);

  invokeInTab("logPointTest");
  info("logPointTest invoked in tab");
  await waitForPaused(dbg);
  await resume(dbg);

  info("Checking for any console messages");
  await hasConsoleMessage(dbg, "stacktrace test");

  const [stacktraceMsg] = await findConsoleMessages(
    dbg.toolbox,
    "stacktrace test"
  );
  const stacktraceFrames = await waitFor(() =>
    stacktraceMsg.querySelector(".frames")
  );

  const frameNodes = stacktraceFrames.querySelectorAll(".frame");
  info(`Found ${frameNodes.length} frames in the stacktrace`);

  is(
    frameNodes.length,
    2,
    "The message does have the expected number of frames in the stacktrace"
  );
  ok(
    frameNodes[0].textContent.includes("script-switching-01.js:6"),
    "First frame should be from line 6"
  );
  ok(
    frameNodes[1].textContent.includes("script-switching-01.js:12"),
    "Second frame should be from line 12"
  );

  // reopen the panel
  info("Reopening the panel to test the checkbox");
  await selectSource(dbg, "script-switching-01.js");

  rightClickElement(dbg, "gutterElement", 8);
  await waitForContextMenu(dbg);
  selectDebuggerContextMenuItem(
    dbg,
    `${selectors.addLogItem},${selectors.editLogItem}`
  );
  await waitForConditionalPanelFocus(dbg);

  info("Updating the log point input value");
  type(dbg, "'logpoint without stacktrace'");
  // wait for a bit so codemirror is able to process the input
  await wait(1000);

  //make sure that the checkbox is checked
  const stacktraceCheckbox = dbg.win.document.querySelector("#showStacktrace");
  ok(
    stacktraceCheckbox.checked,
    "Checkbox is still checked when reopening the logpoint panel"
  );

  // uncheck the checkbox
  info("Click the checkbox to uncheck it");
  stacktraceCheckbox.click();
  info("Checkbox clicked to uncheck it");
  await waitFor(() => {
    return !stacktraceCheckbox.checked;
  });

  ok(true, "Checkbox is unchecked after clicking");

  const saveButton = dbg.win.document.getElementById("save-logpoint");
  const onBreakpointSet = waitForDispatch(dbg.store, "SET_BREAKPOINT");
  saveButton.click();
  await onBreakpointSet;

  info("Call logPointTest to hit the logpoint");
  invokeInTab("logPointTest");
  // the functions has a debugger statement, so wait for pause and resume
  await waitForPaused(dbg);
  await resume(dbg);

  await hasConsoleMessage(dbg, "logpoint without stacktrace");
  const [logpointMsg] = await findConsoleMessages(
    dbg.toolbox,
    "logpoint without stacktrace"
  );
  // Wait for a bit so stacktrace would be rendered
  await wait(1000);
  is(
    logpointMsg.querySelector(".frames"),
    null,
    "There is no stacktrace for the logpoint without stacktrace"
  );
});
