/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/debugger/test/mochitest/shared-head.js",
  this
);

const TAB_URL =
  "data:text/html,<script>function foo() { bar(); }; function bar() {}</script>";

/* import-globals-from helper-collapsibilities.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "helper-collapsibilities.js",
  this
);

/**
 * Test JavaScript Tracer in about:devtools-toolbox tabs (ie non localTab tab target).
 */
add_task(async function () {
  // This is preffed off for now, so ensure turning it on
  await pushPref("devtools.debugger.features.javascript-tracing", true);

  const testTab = await addTab(TAB_URL);

  info("Force all debug target panes to be expanded");
  prepareCollapsibilitiesTest();

  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);
  const { devtoolsTab, devtoolsWindow } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window,
    TAB_URL
  );
  info("Select performance panel");
  const toolbox = getToolbox(devtoolsWindow);
  await toolbox.selectTool("jsdebugger");

  info("Add a breakpoint at line 10 in the test script");
  const debuggerContext = createDebuggerContext(toolbox);

  await toggleJsTracerMenuItem(
    debuggerContext,
    "#jstracer-menu-item-debugger-sidebar"
  );

  await toggleJsTracer(toolbox);

  info("Invoke some code that will be traced");
  await ContentTask.spawn(testTab.linkedBrowser, {}, function () {
    content.wrappedJSObject.foo();
  });

  info("Wait for the expected traces to appear in the call tree");
  const tree = await waitForElementWithSelector(
    debuggerContext,
    "#tracer-tab-panel .tree"
  );
  const traces = await waitFor(() => {
    const elements = tree.querySelectorAll(
      ".trace-line .frame-link-function-display-name"
    );
    if (elements.length == 2) {
      return elements;
    }
    return false;
  });
  is(traces[0].textContent, "λ foo");
  is(traces[1].textContent, "λ bar");

  await closeAboutDevtoolsToolbox(document, devtoolsTab, window);
  await removeTab(testTab);
  await removeTab(tab);
});
