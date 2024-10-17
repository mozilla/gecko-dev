/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-serviceworker.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "helper-serviceworker.js",
  this
);

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/debugger/test/mochitest/shared-head.js",
  this
);

const SW_TAB_URL =
  URL_ROOT_SSL + "resources/service-workers/controlled-sw.html";
const SW_URL =
  URL_ROOT_SSL + "resources/service-workers/controlled-sw.worker.js";

/**
 * Test various simple debugging operation against service workers debugged through about:debugging.
 */
add_task(async function () {
  await enableServiceWorkerDebugging();
  await pushPref("devtools.toolbox.splitconsoleHeight", 400);

  const { document, tab, window } = await openAboutDebugging({
    enableWorkerUpdates: true,
  });
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  // Open a tab that registers a basic service worker.
  const swTab = await addTab(SW_TAB_URL);

  // Wait for the registration to make sure service worker has been started, and that we
  // are not just reading STOPPED as the initial state.
  await waitForRegistration(swTab);

  info("Open a toolbox to debug the worker");
  const { devtoolsTab, devtoolsWindow } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window,
    SW_URL
  );

  const toolbox = getToolbox(devtoolsWindow);

  info("Assert the default tools displayed in worker toolboxes");
  const toolTabs = toolbox.doc.querySelectorAll(".devtools-tab");
  const activeTools = [...toolTabs].map(toolTab =>
    toolTab.getAttribute("data-id")
  );

  is(
    activeTools.join(","),
    "webconsole,jsdebugger",
    "Correct set of tools supported by worker"
  );

  const webconsole = await toolbox.selectTool("webconsole");
  const { hud } = webconsole;

  info(
    "Checked that (cached) messages logged before opening the toolbox are displayed"
  );
  await waitFor(() =>
    findMessageByType(hud, "service worker installed", ".log")
  );

  info("Evaluate location in the console");
  await executeAndWaitForMessage(hud, "this.location.toString()", SW_URL);
  ok(true, "Got the location logged in the console");

  info(
    "Evaluate Date and RegExp to ensure their formater also work from worker threads"
  );
  await executeAndWaitForMessage(
    hud,
    "new Date(2013, 3, 1)",
    "Mon Apr 01 2013 00:00:00"
  );
  ok(true, "Date object has expected text content");
  await executeAndWaitForMessage(hud, "new RegExp('.*')", "/.*/");
  ok(true, "RegExp has expected text content");
  await executeAndWaitForMessage(
    hud,
    "console.log(globalThis)",
    "ServiceWorkerGlobalScope",
    ".log"
  );
  ok(true, "console.log has expected text content");

  await toolbox.selectTool("jsdebugger");
  const dbg = createDebuggerContext(toolbox);

  info("Trigger some code in the worker and wait for pause");
  await SpecialPowers.spawn(swTab.linkedBrowser, [], async function () {
    content.wrappedJSObject.installServiceWorkerAndPause();
  });
  await waitForPaused(dbg);
  ok(true, "successfully paused");

  info(
    "Evaluate some variable only visible if we execute in the breakpoint frame"
  );
  // open split console
  const onSplitConsole = toolbox.once("split-console");
  EventUtils.synthesizeKey("VK_ESCAPE", {}, devtoolsWindow);
  await onSplitConsole;
  await executeAndWaitForMessage(
    hud,
    "event.data",
    "install-service-worker-and-pause"
  );

  info("Resume execution");
  await resume(dbg);

  info("Test pausing from console evaluation");
  hud.ui.wrapper.dispatchEvaluateExpression("debugger; 42");
  await waitForPaused(dbg);
  ok(true, "successfully paused");
  info("Immediately resume");
  await resume(dbg);
  await waitFor(() => findMessageByType(hud, "42", ".result"));
  ok("The paused console evaluation resumed and logged its magic number");

  info("Destroy the toolbox");
  await closeAboutDevtoolsToolbox(document, devtoolsTab, window);

  info("Unregister service worker");
  await unregisterServiceWorker(swTab);

  info("Wait until the service worker disappears from about:debugging");
  await waitUntil(() => !findDebugTargetByText(SW_URL, document));

  info("Remove tabs");
  await removeTab(swTab);
  await removeTab(tab);
});

async function executeAndWaitForMessage(
  hud,
  evaluationString,
  expectedResult,
  messageClass = ".result"
) {
  hud.ui.wrapper.dispatchEvaluateExpression(evaluationString);
  await waitFor(
    () => findMessageByType(hud, expectedResult, messageClass),
    "",
    100
  );
}
