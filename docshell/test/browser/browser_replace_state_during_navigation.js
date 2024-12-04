"use strict";

const TEST_URI =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "dummy_page.html";
const TEST_URI_2 =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "file_replace_state_during_navigation.html";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.navigation.requireUserInteraction", true]],
  });
});

add_task(async () => {
  await BrowserTestUtils.withNewTab(TEST_URI, async browser => {
    // Add user interaction to the first page.
    await BrowserTestUtils.synthesizeMouseAtCenter("body", {}, browser);

    // Follow link to the next page.
    await followLink(TEST_URI_2);

    // Navigate, causing a hashchange event to fire and call history.replaceState
    let loaded = BrowserTestUtils.waitForLocationChange(
      gBrowser,
      TEST_URI_2 + "#1"
    );
    await BrowserTestUtils.synthesizeMouseAtCenter("#link", {}, browser);
    await loaded;

    await assertMenulist([TEST_URI_2 + "#1", TEST_URI_2, TEST_URI]);
  });
});
