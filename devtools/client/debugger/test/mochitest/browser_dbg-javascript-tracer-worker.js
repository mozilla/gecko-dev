/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests tracing web workers

"use strict";
add_task(async function testTracingWorker() {
  await pushPref("devtools.debugger.features.javascript-tracing", true);

  // We have to enable worker targets and disable this pref to have functional tracing for workers
  await pushPref("dom.worker.console.dispatch_events_to_main_thread", false);

  const dbg = await initDebugger("doc-scripts.html");

  // This test covers the Web Console, whereas it is no longer the default output
  await toggleJsTracerMenuItem(dbg, "#jstracer-menu-item-console");

  info("Instantiate a worker");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    content.worker = new content.Worker("simple-worker.js");
  });

  await waitFor(
    () => findAllElements(dbg, "threadsPaneItems").length == 2,
    "Wait for the two threads to be displayed in the thread pane"
  );
  const threadsEl = findAllElements(dbg, "threadsPaneItems");
  is(threadsEl.length, 2, "There are two threads in the thread panel");

  info("Enable tracing on all threads");
  await toggleJsTracer(dbg.toolbox);

  // `timer` is called within the worker via a setInterval of 1 second
  await hasConsoleMessage(dbg, "setIntervalCallback");
  await hasConsoleMessage(dbg, "λ timer");

  // Also verify that postMessage are traced
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    content.worker.postMessage("foo");
  });

  await hasConsoleMessage(dbg, "DOM | global.message");
  await hasConsoleMessage(dbg, "λ onmessage");

  await dbg.toolbox.closeToolbox();
});
