/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://mochi.test:8888"
);

const URL = `${ROOT}/dummy_page.html#:~:text=dummy`;

function waitForPageShow(browser) {
  return BrowserTestUtils.waitForContentEvent(browser, "pageshow", true);
}

add_task(async function test_fragment_restore_urlbar() {
  await BrowserTestUtils.withNewTab("https://example.com", async browser => {
    let loaded = BrowserTestUtils.browserLoaded(browser, false);
    BrowserTestUtils.startLoadingURIString(browser, URL);
    await loaded;

    // Go back in history.
    let change = waitForPageShow(browser);
    browser.goBack();
    await change;
    change = waitForPageShow(browser);
    // Go forward in history.
    browser.goForward();
    await change;
    is(gURLBar.inputField.value, URL, "URL should have text directive");
  });
});
