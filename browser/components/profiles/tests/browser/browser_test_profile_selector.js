/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_selector_window() {
  if (!AppConstants.MOZ_SELECTABLE_PROFILES) {
    // `mochitest-browser` suite `add_task` does not yet support
    // `properties.skip_if`.
    ok(true, "Skipping because !AppConstants.MOZ_SELECTABLE_PROFILES");
    return;
  }

  let profile = await setupMockDB();

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:profilemanager",
    },
    async browser => {
      await SpecialPowers.spawn(
        browser,
        [{ path: profile.path }],
        async aProfile => {
          // mock() returns an object with a fake `runw` method that, when
          // called, records its arguments.
          let input = [];
          let mock = () => {
            return {
              runw: (...args) => {
                input.push(...args);
              },
            };
          };

          content.window.close = () => {}; // Do nothing so the window doesn't close during the test

          const profileSelector =
            content.document.querySelector("profile-selector");
          await profileSelector.updateComplete;

          profileSelector.selectableProfileService.getExecutableProcess = mock;

          const profiles = profileSelector.profileCards;

          Assert.equal(profiles.length, 1, "There is one profile card");
          Assert.ok(
            profileSelector.createProfileCard,
            "The create profile card exists"
          );
          profileSelector.profileCards[0].click();

          let expected;
          if (Services.appinfo.OS === "Darwin") {
            expected = ["-foreground", "--profile", aProfile.path];
          } else {
            expected = ["--profile", aProfile.path];
          }

          Assert.deepEqual(input[1], expected, "Expected runw arguments");
        }
      );
    }
  );
});
