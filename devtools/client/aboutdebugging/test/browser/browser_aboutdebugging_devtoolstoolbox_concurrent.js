/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/debugger/test/mochitest/shared-head.js",
  this
);

const TAB1_URL =
  "data:text/html," +
  encodeURIComponent("<script>function firstTab() {}</script>First tab");
const TAB2_URL =
  "data:text/html," +
  encodeURIComponent("<script>function secondTab() {}</script>Second tab");

/* import-globals-from helper-collapsibilities.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "helper-collapsibilities.js",
  this
);

const RUNTIME_ID = "1337id";
const DEVICE_NAME = "Fancy Phone";
const RUNTIME_NAME = "Lorem ipsum";

/**
 * Test JavaScript Tracer in about:devtools-toolbox tabs (ie non localTab tab target).
 */
add_task(async function () {
  const testTab1 = await addTab(TAB1_URL);
  const testTab2 = await addTab(TAB2_URL);

  info("Force all debug target panes to be expanded");
  prepareCollapsibilitiesTest();

  const { disconnect, mocks } = await connectToLocalFirefox({
    runtimeId: RUNTIME_ID,
    runtimeName: RUNTIME_NAME,
    deviceName: DEVICE_NAME,
  });

  const { document, tab, window } = await openAboutDebugging();

  mocks.emitUSBUpdate();
  await connectToRuntime(DEVICE_NAME, document);
  await selectRuntime(DEVICE_NAME, RUNTIME_NAME, document);

  //await selectThisFirefoxPage(document, window.AboutDebugging.store);

  info("Open a first remote toolbox on the debugger for the first tab");
  const { devtoolsTab: devtoolsTab1, devtoolsWindow: devtoolsWindow1 } =
    await openAboutDevtoolsToolbox(document, tab, window, TAB1_URL);
  const toolbox1 = getToolbox(devtoolsWindow1);
  await toolbox1.selectTool("jsdebugger");

  info("Assert we can see first tab sources in the debugger");
  const debuggerContext1 = createDebuggerContext(toolbox1);
  await waitForSources(debuggerContext1, TAB1_URL);
  await selectSource(debuggerContext1, TAB1_URL);

  info("Open a second remote toolbox on the debugger for the second tab");
  const { devtoolsTab: devtoolsTab2, devtoolsWindow: devtoolsWindow2 } =
    await openAboutDevtoolsToolbox(document, tab, window, TAB2_URL);
  const toolbox2 = getToolbox(devtoolsWindow2);
  await toolbox2.selectTool("jsdebugger");

  info("Assert we can see second tab sources in the debugger");
  const debuggerContext2 = createDebuggerContext(toolbox2);
  await waitForSources(debuggerContext2, TAB2_URL);
  await selectSource(debuggerContext2, TAB2_URL);

  await closeAboutDevtoolsToolbox(document, devtoolsTab1, window);
  await closeAboutDevtoolsToolbox(document, devtoolsTab2, window);

  await disconnect(document);
  await removeTab(testTab1);
  await removeTab(testTab2);
  await removeTab(tab);
});
