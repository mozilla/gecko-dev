/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/*
 * Tests toggling the skip pausing button and
 * invoking functions without pausing.
 */

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-scripts.html");
  await selectSource(dbg, "simple3.js");

  info("Adding a breakpoint should remove the skipped pausing state");
  await skipPausing(dbg);
  await waitForState(dbg, () => dbg.selectors.getSkipPausing());
  await addBreakpointViaGutter(dbg, 2);
  await waitForState(dbg, () => !dbg.selectors.getSkipPausing());
  invokeInTab("simple");
  await waitForPaused(dbg);
  ok(true, "The breakpoint has been hit after a breakpoint was created");
  await resume(dbg);

  info("Toggling off breakpoint should not remove the skipped pausing state");
  await skipPausing(dbg);
  ok(dbg.selectors.getSkipPausing());
  await removeBreakpointViaGutter(dbg, 2);
  ok(dbg.selectors.getSkipPausing());

  info("Toggling on breakpoint should remove the skipped pausing state");
  await addBreakpointViaGutter(dbg, 2);
  await waitForState(dbg, () => !dbg.selectors.getSkipPausing());
  invokeInTab("simple");
  await waitForPaused(dbg);
  ok(true, "The breakpoint has been hit after the breakpoint was re-enabled");
  await resume(dbg);

  info("Disabling a breakpoint should not remove the skipped pausing state");
  await skipPausing(dbg);
  await disableBreakpoint(dbg, 0);
  ok(dbg.selectors.getSkipPausing());
  invokeInTab("simple");
  assertNotPaused(dbg);

  info("Skip pausing should not be reset on page reload");
  await reload(dbg);
  ok(dbg.selectors.getSkipPausing());
});

function skipPausing(dbg) {
  clickElementWithSelector(dbg, ".command-bar-skip-pausing");
  return waitForState(dbg, () => dbg.selectors.getSkipPausing());
}

function toggleBreakpoint(dbg, index) {
  const breakpoints = findAllElements(dbg, "breakpointItems");
  const bp = breakpoints[index];
  const input = bp.querySelector("input");
  input.click();
}

async function disableBreakpoint(dbg, index) {
  const disabled = waitForDispatch(dbg.store, "SET_BREAKPOINT");
  toggleBreakpoint(dbg, index);
  await disabled;
}
