/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests tracing argument values

"use strict";
add_task(async function testTracingValues() {
  await pushPref("devtools.debugger.features.javascript-tracing", true);

  // Cover tracing function argument values
  const jsCode = `function foo() { bar(1, ["array"], { attribute: 3 }, BigInt(4), Infinity, Symbol("6"), "7"); }; function bar(a, b, c) {}`;
  const dbg = await initDebuggerWithAbsoluteURL(
    "data:text/html," + encodeURIComponent(`<script>${jsCode}</script>`)
  );

  // This test covers the Web Console, whereas it is no longer the default output
  await toggleJsTracerMenuItem(dbg, "#jstracer-menu-item-console");

  await toggleJsTracerMenuItem(dbg, "#jstracer-menu-item-log-values");

  await toggleJsTracer(dbg.toolbox);

  invokeInTab("foo");

  await hasConsoleMessage(dbg, "λ foo()");
  await hasConsoleMessage(dbg, "λ bar");
  const { value } = await findConsoleMessage(dbg, "λ bar");
  is(
    value,
    `⟶ interpreter λ bar(1, \nArray [ "array" ]\n, \nObject { attribute: 3 }\n, 4n, Infinity, Symbol("6"), "7")`,
    "The argument were printed for bar()"
  );

  // Reset the log values setting
  Services.prefs.clearUserPref("devtools.debugger.javascript-tracing-values");
});
