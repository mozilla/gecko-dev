/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint-disable max-nested-callbacks */

const ASSET_BASE_URL =
  "https://example.com/browser/devtools/client/performance-new/test/browser/webchannel-open-script-in-debugger_assets/";
const TEST_PAGE_URL = ASSET_BASE_URL + "test-page.html";
const TEST_SCRIPT_URL = ASSET_BASE_URL + "test-script.js";
const FRONTEND_URL = ASSET_BASE_URL + "webchannel.html";

const {
  gDevTools,
} = require("resource://devtools/client/framework/devtools.js");

add_task(async function test() {
  info(
    "Test the WebChannel mechanism works for opening the devtools debugger with the script"
  );

  // First open the test page so it can load a simple script.
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE_URL,
    },
    async browser => {
      const testPageTabId = browser.browsingContext.browserId;
      const request = {
        tabId: testPageTabId,
        scriptUrl: TEST_SCRIPT_URL,
        line: 1,
        column: 1,
      };

      // Make sure that the current active tab is the test page for a sanity check.
      Assert.strictEqual(
        gBrowser.selectedBrowser.browsingContext.browserId,
        testPageTabId,
        "Current selected tab is the test page."
      );

      const onToolboxReady = gDevTools.once("toolbox-ready");
      // Now open the second tab that will send the webchannel request.
      // After the request it must switch to the first tab, and then
      await BrowserTestUtils.withNewTab(
        {
          gBrowser,
          url:
            FRONTEND_URL +
            "?request=" +
            encodeURIComponent(JSON.stringify(request)),
        },
        async () => {
          // Note that we could do a sanity check here for the selected tab, but
          // it will be racy in case the code inside the html page gets executed
          // quickly. So it's better to not do it.
          await onToolboxReady;
          ok(true, "Toolbox is successfully loaded");

          // Check that the active tab is back to the first tab again.
          Assert.strictEqual(
            gBrowser.selectedBrowser.browsingContext.browserId,
            testPageTabId,
            "Selected tab should be switched back to the test page again."
          );

          // Now check if the debugger successfully loaded the source.
          const toolbox = gDevTools.getToolboxForTab(gBrowser.selectedTab);
          ok(!!toolbox, "Toolbox is opened successfully");

          const dbg = toolbox.getPanel("jsdebugger");
          await waitUntil(() => {
            const source = dbg._selectors.getSelectedSource(dbg._getState());
            return source && source.url === TEST_SCRIPT_URL;
          });

          ok(true, "Source is successfully loaded.");
        }
      );
    }
  );
});
