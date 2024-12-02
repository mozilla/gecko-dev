/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";
// Test enabling and disabling a breakpoint using the check boxes
add_task(async function testEnableDisableBreakpoints() {
  const dbg = await initDebugger("doc-scripts.html", "simple2.js");

  // Create two breakpoints
  await selectSource(dbg, "simple2.js");
  await addBreakpoint(dbg, "simple2.js", 3);
  await addBreakpoint(dbg, "simple2.js", 5);

  // Disable the first one
  await disableBreakpoint(dbg, 0);
  let bp1 = findBreakpoint(dbg, "simple2.js", 3);
  let bp2 = findBreakpoint(dbg, "simple2.js", 5);
  is(bp1.disabled, true, "first breakpoint is disabled");
  is(bp2.disabled, false, "second breakpoint is enabled");

  // Disable and Re-Enable the second one
  await disableBreakpoint(dbg, 1);
  await enableBreakpoint(dbg, 1);
  bp2 = findBreakpoint(dbg, "simple2.js", 5);
  is(bp2.disabled, false, "second breakpoint is enabled");

  // Cleanup
  await cleanupBreakpoints(dbg);

  // Test enabling and disabling a breakpoint using the context menu
  await selectSource(dbg, "simple2.js");
  await addBreakpoint(dbg, "simple2.js", 3);
  await addBreakpoint(dbg, "simple2.js", 5);

  assertBreakpointSnippet(dbg, 3, "return x + y;");

  rightClickElement(dbg, "breakpointItem", 2);
  await waitForContextMenu(dbg);
  const disableBreakpointDispatch = waitForDispatch(
    dbg.store,
    "SET_BREAKPOINT"
  );
  selectContextMenuItem(dbg, selectors.breakpointContextMenu.disableSelf);
  await disableBreakpointDispatch;

  bp1 = findBreakpoint(dbg, "simple2.js", 3);
  bp2 = findBreakpoint(dbg, "simple2.js", 5);
  is(bp1.disabled, true, "first breakpoint is disabled");
  is(bp2.disabled, false, "second breakpoint is enabled");

  rightClickElement(dbg, "breakpointItem", 2);
  await waitForContextMenu(dbg);
  const enableBreakpointDispatch = waitForDispatch(dbg.store, "SET_BREAKPOINT");
  selectContextMenuItem(dbg, selectors.breakpointContextMenu.enableSelf);
  await enableBreakpointDispatch;

  bp1 = findBreakpoint(dbg, "simple2.js", 3);
  bp2 = findBreakpoint(dbg, "simple2.js", 5);
  is(bp1.disabled, false, "first breakpoint is enabled");
  is(bp2.disabled, false, "second breakpoint is enabled");

  // Cleanup
  await cleanupBreakpoints(dbg);

  // Test creation of disabled breakpoint with shift-click
  await shiftClickElement(dbg, "gutterElement", 3);
  await waitForBreakpoint(dbg, "simple2.js", 3);

  const bp = findBreakpoint(dbg, "simple2.js", 3);
  is(bp.disabled, true, "breakpoint is disabled");

  // Cleanup
  await cleanupBreakpoints(dbg);
});

// Test the keyboard events on the breakpoint list
add_task(async function testBreakpointsKeyboardEvents() {
  const dbg = await initDebugger("doc-scripts.html", "simple2.js");

  // Create two breakpoints
  await selectSource(dbg, "simple2.js");

  info("Add two breakpoints");
  await addBreakpoint(dbg, "simple2.js", 3);
  await addBreakpoint(dbg, "simple2.js", 4);

  info("Add conditional breakpoint to line 5");
  setEditorCursorAt(dbg, 4, 1);
  await setConditionalBreakpointWithKeyboardShortcut(dbg, "3");
  pressKey(dbg, "Enter");
  await waitForCondition(dbg, "3");

  info("Focus and select first breakpoint with the keyboard shortcut");
  let bp = findAllElements(dbg, "breakpointItems")[0];
  bp.focus();
  pressKey(dbg, "Enter");

  info("Wait for line with the first breakpoint to be selected");
  await waitFor(() => dbg.selectors.getSelectedLocation().line == 3);

  info("Focus and select second breakpoint with the keyboard shortcut");
  bp = findAllElements(dbg, "breakpointItems")[1];
  bp.focus();
  pressKey(dbg, "Space");

  info("Wait for line with the second breakpoint to be selected");
  await waitFor(() => dbg.selectors.getSelectedLocation().line == 4);

  info(
    "Focus and select and update conditional breakpoint with the keyboard shortcut"
  );
  bp = findAllElements(dbg, "breakpointItems")[2];
  bp.focus();
  pressKey(dbg, "ShiftEnter");
  typeInPanel(dbg, "5");
  await waitForCondition(dbg, "35");

  // Cleanup
  await cleanupBreakpoints(dbg);
});

function toggleBreakpoint(dbg, index) {
  const bp = findAllElements(dbg, "breakpointItems")[index];
  const input = bp.querySelector("input");
  input.click();
}

async function disableBreakpoint(dbg, index) {
  const disabled = waitForDispatch(dbg.store, "SET_BREAKPOINT");
  toggleBreakpoint(dbg, index);
  await disabled;
}

async function enableBreakpoint(dbg, index) {
  const enabled = waitForDispatch(dbg.store, "SET_BREAKPOINT");
  toggleBreakpoint(dbg, index);
  await enabled;
}

async function cleanupBreakpoints(dbg) {
  ok(
    findBreakpoint(dbg, "simple2.js", 3),
    "Breakpoint on line 3 exists before trying to remove it"
  );
  let dispatched = waitForDispatch(dbg.store, "REMOVE_BREAKPOINT");
  clickElement(dbg, "gutterElement", 3);
  await dispatched;
  await waitForBreakpointRemoved(dbg, "simple2.js", 3);

  // Breakpoint on line 5 doesn't always exists
  if (!findBreakpoint(dbg, "simple2.js", 5)) {
    return;
  }
  dispatched = waitForDispatch(dbg.store, "REMOVE_BREAKPOINT");
  clickElement(dbg, "gutterElement", 5);
  await dispatched;
  await waitForBreakpointRemoved(dbg, "simple2.js", 5);
}
