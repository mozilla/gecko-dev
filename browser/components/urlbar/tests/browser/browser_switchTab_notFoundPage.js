/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This test ensures that switch to tab results appear even if the page was
 * an error page, that would normally be excluded from history results.
 */

"use strict";

add_task(async function test_switchTab_notFoundPage() {
  registerCleanupFunction(PlacesUtils.history.clear);
  for (let switchTabsEnabled of [true, false]) {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.urlbar.suggest.openpage", switchTabsEnabled]],
    });
    const initialTab = gBrowser.selectedTab;
    const url = "https://example.com/notFoundPage.html";
    await BrowserTestUtils.withNewTab({ gBrowser, url }, async () => {
      // Switch back as we don't show switching to the current tab.
      await BrowserTestUtils.switchTab(gBrowser, initialTab);
      let context = await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: "notFoundPage",
      });
      if (switchTabsEnabled) {
        Assert.ok(
          context.results.some(
            result =>
              result.payload.url == url &&
              result.type == UrlbarUtils.RESULT_TYPE.TAB_SWITCH
          ),
          "A switch to tab entry should be present for the URL"
        );
      } else {
        Assert.ok(
          !context.results.some(result => result.payload.url == url),
          "The URL should not appear in results"
        );
      }
    });
  }
});
