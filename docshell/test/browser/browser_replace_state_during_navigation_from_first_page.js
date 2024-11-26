"use strict";

const TEST_URI =
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

    // Navigate, causing a hashchange event to fire and call history.replaceState
    await BrowserTestUtils.synthesizeMouseAtCenter("#link", {}, browser);

    await assertMenulist([TEST_URI + "#1", TEST_URI + "#inject", TEST_URI]);
  });
});
