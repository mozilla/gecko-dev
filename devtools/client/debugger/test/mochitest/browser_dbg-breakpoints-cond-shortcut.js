/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test opening conditional panel using keyboard shortcut.
// Should access the closest breakpoint to a passed in cursorPosition.

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-scripts.html", "long.js");

  await selectSource(dbg, "long.js");
  await waitForSelectedSource(dbg, "long.js");
  // Wait a bit for CM6 to complete any updates so the conditional panel
  // does not lose focus after the it has been opened
  await waitForDocumentLoadComplete(dbg);
  info(
    "toggle conditional panel with shortcut: no breakpoints, default cursorPosition"
  );
  pressKey(dbg, "toggleCondPanel");
  await waitForConditionalPanelFocus(dbg);

  ok(
    !!(await getConditionalPanelAtLine(dbg, 1)),
    "conditional panel panel is open on line 1"
  );
  is(
    dbg.selectors.getConditionalPanelLocation().line,
    1,
    "conditional panel location is line 1"
  );
  info("close conditional panel");
  pressKey(dbg, "Escape");

  info(
    "toggle conditional panel with shortcut: cursor on line 32, no breakpoints"
  );
  await selectSource(dbg, "long.js", 32, 2);
  pressKey(dbg, "toggleCondPanel");

  await waitForConditionalPanelFocus(dbg);
  ok(
    !!(await getConditionalPanelAtLine(dbg, 32)),
    "conditional panel is open on line 32"
  );
  is(
    dbg.selectors.getConditionalPanelLocation().line,
    32,
    "conditional panel location is line 32"
  );
  info("close conditional panel");
  pressKey(dbg, "Escape");

  info("add active column breakpoint on line 32 and set cursorPosition");
  await selectSource(dbg, "long.js", 32, 2);
  await enableFirstColumnBreakpoint(dbg);
  info(
    "toggle conditional panel with shortcut and add condition to first breakpoint"
  );
  await setConditionalBreakpointWithKeyboardShortcut(dbg, "1");
  await waitForCondition(dbg, 1);
  const firstBreakpoint = findColumnBreakpoint(dbg, "long.js", 32, 2);
  is(
    firstBreakpoint.options.condition,
    "1",
    "first breakpoint created with condition using shortcut"
  );

  info("set cursor at second breakpoint position and activate breakpoint");
  await selectSource(dbg, "long.js", 32, 26);
  await enableSecondColumnBreakpoint(dbg);
  info(
    "toggle conditional panel with shortcut and add condition to second breakpoint"
  );
  await setConditionalBreakpointWithKeyboardShortcut(dbg, "2");
  await waitForCondition(dbg, 2);
  const secondBreakpoint = findColumnBreakpoint(dbg, "long.js", 32, 26);
  is(
    secondBreakpoint.options.condition,
    "2",
    "second breakpoint created with condition using shortcut"
  );

  info(
    "set cursor position near first breakpoint, toggle conditional panel and edit breakpoint"
  );
  await selectSource(dbg, "long.js", 32, 8);
  info("toggle conditional panel and edit condition using shortcut");
  await setConditionalBreakpointWithKeyboardShortcut(dbg, "2");
  ok(
    !!waitForCondition(dbg, "12"),
    "breakpoint closest to cursor position has been edited"
  );

  info("close conditional panel");
  pressKey(dbg, "Escape");

  info(
    "set cursor position near second breakpoint, toggle conditional panel and edit breakpoint"
  );
  await selectSource(dbg, "long.js", 32, 22);
  info("toggle conditional panel and edit condition using shortcut");
  await setConditionalBreakpointWithKeyboardShortcut(dbg, "3");
  ok(
    !!waitForCondition(dbg, "13"),
    "breakpoint closest to cursor position has been edited"
  );

  info("close conditional panel");
  pressKey(dbg, "Escape");

  info("toggle log panel with shortcut: cursor on line 33");

  await selectSource(dbg, "long.js", 34, 2);
  await setLogBreakpointWithKeyboardShortcut(dbg, "3");
  ok(
    !!waitForLog(dbg, "3"),
    "breakpoint closest to cursor position has been edited"
  );

  info("close conditional panel");
  pressKey(dbg, "Escape");
});

// from browser_dbg-breakpoints-columns.js
async function enableFirstColumnBreakpoint(dbg) {
  await addBreakpoint(dbg, "long.js", 32);
  const bpMarkers = await waitForAllElements(dbg, "columnBreakpoints");

  Assert.strictEqual(bpMarkers.length, 2, "2 column breakpoints");
  assertClass(bpMarkers[0], "active");
  assertClass(bpMarkers[1], "active", false);
}

async function enableSecondColumnBreakpoint(dbg) {
  let bpMarkers = await waitForAllElements(dbg, "columnBreakpoints");

  bpMarkers[1].click();
  await waitForBreakpointCount(dbg, 2);

  bpMarkers = findAllElements(dbg, "columnBreakpoints");
  assertClass(bpMarkers[1], "active");
  await waitForAllElements(dbg, "breakpointItems", 2);
}

function setLogBreakpointWithKeyboardShortcut(dbg, condition) {
  pressKey(dbg, "toggleLogPanel");
  return typeInPanel(dbg, condition, true);
}
