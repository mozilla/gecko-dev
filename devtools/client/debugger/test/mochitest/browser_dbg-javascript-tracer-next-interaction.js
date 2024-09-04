/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests tracing only on next user interaction

"use strict";

add_task(async function testTracingOnNextInteraction() {
  await pushPref("devtools.debugger.features.javascript-tracing", true);

  // Cover tracing only on next user interaction
  const jsCode = `function foo() {}; window.addEventListener("mousedown", function onmousedown(){}, { capture: true }); window.onclick = function onclick() {};`;
  const dbg = await initDebuggerWithAbsoluteURL(
    "data:text/html," +
      encodeURIComponent(`<script>${jsCode}</script><body></body>`)
  );

  // This test covers the Web Console, whereas it is no longer the default output
  await toggleJsTracerMenuItem(dbg, "#jstracer-menu-item-console");

  await toggleJsTracerMenuItem(dbg, "#jstracer-menu-item-next-interaction");

  await toggleJsTracer(dbg.toolbox);

  const traceButton = dbg.toolbox.doc.getElementById("command-button-jstracer");
  // Wait for the trace button to be highlighted
  await waitFor(() => {
    return traceButton.classList.contains("pending");
  });
  ok(
    traceButton.classList.contains("pending"),
    "The tracer button is also highlighted as pending until the user interaction is triggered"
  );

  invokeInTab("foo");

  // Let a change to have the tracer to regress and log foo call
  await wait(500);

  is(
    (await findConsoleMessages(dbg.toolbox, "λ foo")).length,
    0,
    "The tracer did not log the function call before trigerring the click event"
  );

  // We intentionally turn off this a11y check, because the following click
  // is send on an empty <body> to to test the click event tracer performance,
  // and not to activate any control, therefore this check can be ignored.
  AccessibilityUtils.setEnv({
    mustHaveAccessibleRule: false,
  });
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "body",
    {},
    gBrowser.selectedBrowser
  );
  AccessibilityUtils.resetEnv();

  await hasConsoleMessage(dbg, "Started tracing to Web Console");

  await hasConsoleMessage(dbg, "λ onmousedown");
  await hasConsoleMessage(dbg, "λ onclick");

  is(
    traceButton.getAttribute("aria-pressed"),
    "true",
    "The tracer button is still highlighted as active"
  );
  ok(
    !traceButton.classList.contains("pending"),
    "The tracer button is no longer pending after the user interaction"
  );

  is(
    (await findConsoleMessages(dbg.toolbox, "λ foo")).length,
    0,
    "Even after the click, the code called before the click is still not logged"
  );

  // But if we call this code again, now it should be logged
  invokeInTab("foo");
  await hasConsoleMessage(dbg, "λ foo");
  ok(true, "foo was traced as expected");

  info("Stop tracing");
  await toggleJsTracer(dbg.toolbox);

  is(
    traceButton.getAttribute("aria-pressed"),
    "false",
    "The tracer button is no longer highlighted as active"
  );
  ok(
    !traceButton.classList.contains("pending"),
    "The tracer button is still not pending after disabling"
  );

  // Reset the trace on next interaction setting
  Services.prefs.clearUserPref(
    "devtools.debugger.javascript-tracing-on-next-interaction"
  );
});

add_task(async function testInteractionBetweenDebuggerAndConsole() {
  const jsCode = `function foo() {};`;
  const dbg = await initDebuggerWithAbsoluteURL(
    "data:text/html," + encodeURIComponent(`<script>${jsCode}</script>`)
  );

  info("Enable the tracing via the toolbox button");
  await toggleJsTracer(dbg.toolbox);

  invokeInTab("foo");

  await hasConsoleMessage(dbg, "λ foo");

  info("Disable the tracing via a console command");
  const { hud } = await dbg.toolbox.getPanel("webconsole");
  let msg = await evaluateExpressionInConsole(hud, ":trace", "console-api");
  is(msg.textContent.trim(), "Stopped tracing");

  const button = dbg.toolbox.doc.getElementById("command-button-jstracer");
  await waitFor(() => !button.classList.contains("checked"));

  info(
    "Clear the console output from the first tracing session started from the debugger"
  );
  hud.ui.clearOutput();
  await waitFor(
    async () => !(await findConsoleMessages(dbg.toolbox, "λ foo")).length,
    "Wait for console to be cleared"
  );

  info("Enable the tracing via a console command");
  msg = await evaluateExpressionInConsole(hud, ":trace", "console-api");
  is(msg.textContent.trim(), "Started tracing to Web Console");

  info("Wait for tracing to be also enabled in toolbox button");
  await waitFor(() => button.classList.contains("checked"));

  invokeInTab("foo");

  await hasConsoleMessage(dbg, "λ foo");

  info("Disable the tracing via the debugger button");
  // togglejsTracer will assert that the console logged the "stopped tracing" message
  await toggleJsTracer(dbg.toolbox);

  info("Wait for tracing to be disabled per toolbox button");
  await waitFor(() => !button.classList.contains("checked"));
});
