"use strict";

const TEST_URI = "https://example.com/";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.navigation.requireUserInteraction", true]],
  });
});

add_task(async () => {
  await BrowserTestUtils.withNewTab(TEST_URI, async browser => {
    // Navigate away, after causing a user interaction.
    await SpecialPowers.spawn(browser, [], async () => {
      content.document.notifyUserGestureActivation();
    });
    await followLink(TEST_URI + "2.html");

    // Navigate again, without causing a user interaction.
    let loaded = BrowserTestUtils.waitForLocationChange(
      gBrowser,
      TEST_URI + "3.html"
    );
    await SpecialPowers.spawn(browser, [], async () => {
      content.history.pushState({}, "", "https://example.com/3.html");
    });
    await loaded;

    // The entry with no interaction shouldn't appear.
    await assertMenulist([TEST_URI + "3.html", TEST_URI]);

    // Go back using history.back, which does not check for user interaction.
    loaded = BrowserTestUtils.waitForLocationChange(
      gBrowser,
      TEST_URI + "2.html"
    );
    await SpecialPowers.spawn(browser, [], async () => {
      content.history.back();
    });
    await loaded;

    // We are back on entry 2, so it should appear in the list.
    await assertMenulist([TEST_URI + "3.html", TEST_URI + "2.html", TEST_URI]);
  });
});
