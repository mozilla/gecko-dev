/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function () {
  const initialTab = BrowserTestUtils.addTab(
    gBrowser,
    "https://example.com/document-builder.sjs?html=test-devtools-hang"
  );
  gBrowser.selectedTab = BrowserTestUtils.addTab(
    gBrowser,
    "https://example.net/document-builder.sjs?html=test-devtools-hang"
  );
  isnot(
    initialTab.linkedBrowser.browsingContext.currentWindowGlobal.osPid,
    gBrowser.selectedBrowser.browsingContext.currentWindowGlobal.osPid,
    "The two tabs are loaded in distinct processes"
  );

  // Freeze the second tab
  const slowScriptDone = SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    function () {
      const start = Date.now();
      while (Date.now() < start + 4000) {
        // block the tab for 4 seconds
      }
    }
  );
  let resumed = false;
  slowScriptDone.then(() => {
    resumed = true;
  });

  // Select the first tab while the second is freezing
  gBrowser.selectedTab = initialTab;

  // Try opening on the first tab, which isn't freezing
  const toolbox = await gDevTools.showToolboxForTab(initialTab, {
    toolId: "webconsole",
  });
  ok(true, "Toolbox successfully opened despite frozen tab in background");
  is(
    resumed,
    false,
    "The background tab is still frozen after opening devtools"
  );
  await slowScriptDone;
  is(resumed, true, "The background tab resumed its executions");
  await toolbox.closeToolbox();
});
