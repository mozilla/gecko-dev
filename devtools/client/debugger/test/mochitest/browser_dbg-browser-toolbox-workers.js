/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that all kinds of workers show up properly in the multiprocess browser
// toolbox.

"use strict";

requestLongerTimeout(4);

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/framework/browser-toolbox/test/helpers-browser-toolbox.js",
  this
);
const WORKER_ESM =
  "chrome://mochitests/content/browser/devtools/client/debugger/test/mochitest/examples/worker-esm.mjs";

add_task(async function () {
  await pushPref("devtools.browsertoolbox.scope", "everything");
  await pushPref("dom.serviceWorkers.enabled", true);
  await pushPref("dom.serviceWorkers.testing.enabled", true);

  const ToolboxTask = await initBrowserToolboxTask();
  await ToolboxTask.spawn(selectors, () => {
    const {
      LocalizationHelper,
    } = require("resource://devtools/shared/l10n.js");
    // We have to expose this symbol as global for waitForSelectedSource
    this.DEBUGGER_L10N = new LocalizationHelper(
      "devtools/client/locales/debugger.properties"
    );
  });
  await ToolboxTask.importFunctions({
    waitUntil,
    waitForAllTargetsToBeAttached,
    createDebuggerContext,
    isWasmBinarySource,
    DEBUGGER_L10N,
    getCM,
    waitForState,
    waitForSelectedSource,
    createLocation,
    getUnicodeUrlPath,
    findSource,
    selectSource,
    assertTextContentOnLine,
    getEditorContent,
  });

  await addTab(`${EXAMPLE_URL}doc-all-workers.html`);

  globalThis.worker = new ChromeWorker(WORKER_ESM, { type: "module" });

  await ToolboxTask.spawn(null, async () => {
    /* global gToolbox */
    await gToolbox.selectTool("jsdebugger");

    const dbg = createDebuggerContext(gToolbox);

    await waitUntil(() => {
      const threads = dbg.selectors.getThreads();
      function hasWorker(workerName) {
        // eslint-disable-next-line max-nested-callbacks
        return threads.some(({ name }) => name == workerName);
      }
      return (
        hasWorker("simple-worker.js") &&
        hasWorker("shared-worker.js") &&
        hasWorker("service-worker.sjs") &&
        hasWorker("worker-esm.mjs")
      );
    });

    await waitForAllTargetsToBeAttached(gToolbox.commands.targetCommand);

    await selectSource(dbg, "worker-esm.mjs");
    assertTextContentOnLine(
      dbg,
      7,
      'console.log("Worker ESM main script", foo);'
    );
    await selectSource(dbg, "worker-esm-dep.mjs");
    assertTextContentOnLine(dbg, 1, 'console.log("Worker ESM dependency");');
  });
  ok(true, "All workers appear in browser toolbox debugger");

  globalThis.worker.terminate();
  delete globalThis.worker;

  invokeInTab("unregisterServiceWorker");

  await ToolboxTask.destroy();
});
