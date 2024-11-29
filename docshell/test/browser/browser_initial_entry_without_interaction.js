"use strict";

const TEST_URI = "https://example.com/";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.navigation.requireUserInteraction", true]],
  });
});

add_task(async () => {
  await BrowserTestUtils.withNewTab(TEST_URI, async () => {
    // Navigate away, without causing a user interaction.
    await followLink(TEST_URI + "2.html");

    // Wait for the session data to be flushed before continuing the test
    await new Promise(resolve =>
      SessionStore.getSessionHistory(gBrowser.selectedTab, resolve)
    );

    // The entry with no interaction shouldn't appear.
    await assertMenulist([TEST_URI + "2.html"]);

    const backButton = document.getElementById("back-button");
    ok(backButton.disabled, "The back button should be disabled.");
  });
});
