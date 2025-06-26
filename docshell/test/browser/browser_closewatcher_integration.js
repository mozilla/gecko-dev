/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PAGE =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "dummy_page.html";
const CLOSEWATCHER_PAGE =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "page_with_closewatcher.html";

const runTest =
  (bool, baseURL = TEST_PAGE) =>
  async () => {
    // Wait for the session data to be flushed before continuing the test
    Services.prefs.setBoolPref("dom.closewatcher.enabled", true);

    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, baseURL);

    await new Promise(resolve =>
      SessionStore.getSessionHistory(gBrowser.selectedTab, resolve)
    );

    // Assert the hasActiveCloseWatcher property
    is(
      gBrowser.selectedBrowser.hasActiveCloseWatcher,
      bool,
      `hasActiveCloseWatcher is ${bool}`
    );

    gBrowser.selectedBrowser.processCloseRequest();

    // CloseWatcher may not be immediately closed as the request is over IPC, so allow some grace
    // by checking every 100ms to see if hasActiveCloseWatcher flips to false.
    {
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      const sleep = ms => new Promise(r => setTimeout(r, ms));
      const hasActiveCloseWatcherEventuallyFalse = (async () => {
        while (gBrowser.selectedBrowser.hasActiveCloseWatcher) {
          await sleep(50);
        }
      })();
      await Promise.race([hasActiveCloseWatcherEventuallyFalse, sleep(3000)]);
    }

    // Assert the hasActiveCloseWatcher property is false after a close request
    is(
      gBrowser.selectedBrowser.hasActiveCloseWatcher,
      false,
      `hasActiveCloseWatcher is false after processCloseRequest`
    );

    BrowserTestUtils.removeTab(tab);

    Services.prefs.clearUserPref("dom.closewatcher.enabled");
  };

add_task(runTest(false, TEST_PAGE));
add_task(runTest(true, CLOSEWATCHER_PAGE));
