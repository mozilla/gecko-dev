/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
});

const NEW_PROFILE_NAME = "This is a new profile name";

add_setup(async () => {
  await initGroupDatabase();
});

const setup = async () => {
  let profile = SelectableProfileService.currentProfile;
  Assert.ok(profile, "Should have a profile now");

  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();
  Services.telemetry.clearEvents();
  return profile;
};

add_task(async function test_new_profile_beforeunload() {
  if (!AppConstants.MOZ_SELECTABLE_PROFILES) {
    // `mochitest-browser` suite `add_task` does not yet support
    // `properties.skip_if`.
    ok(true, "Skipping because !AppConstants.MOZ_SELECTABLE_PROFILES");
    return;
  }

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.profiles.profile-name.updated", false],
      ["dom.require_user_interaction_for_beforeunload", false],
    ],
  });

  let profile = SelectableProfileService.currentProfile;
  Assert.ok(profile, "Should have a profile now");

  // The attempt to delete will fail if there is not another profile.
  let profileData = {
    name: "Other Profile",
    avatar: "book",
    themeId: "default",
    themeFg: "var(--text-color)",
    themeBg: "var(--background-color-box)",
    path: "somewhere",
  };

  await SelectableProfileService.insertProfile(profileData);

  let oldDeleteCurrentProfile = SelectableProfileService.deleteCurrentProfile;
  let deleteCurrentProfileCalled = false;
  SelectableProfileService.deleteCurrentProfile = () => {
    deleteCurrentProfileCalled = true;
    throw new Error("Skip shutdown");
  };

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:newprofile",
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        let newProfileCard =
          content.document.querySelector("new-profile-card").wrappedJSObject;

        await ContentTaskUtils.waitForCondition(
          () => newProfileCard.initialized,
          "Waiting for new-profile-card to be initialized"
        );

        await newProfileCard.updateComplete;

        let nameInput = newProfileCard.nameInput;
        Assert.equal(nameInput.value, "", "Profile name is empty to start");

        let deleteButton = newProfileCard.deleteButton;
        EventUtils.synthesizeMouseAtCenter(deleteButton, {}, content);
      });

      await TestUtils.waitForCondition(() => deleteCurrentProfileCalled);
    }
  );

  SelectableProfileService.deleteCurrentProfile = oldDeleteCurrentProfile;

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_create_profile_name() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.profiles.profile-name.updated", false]],
  });

  let profile = await setup();
  is(
    null,
    Glean.profilesNew.name.testGetValue(),
    "We have not recorded any Glean data yet"
  );

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:newprofile",
    },
    async browser => {
      await SpecialPowers.spawn(
        browser,
        [NEW_PROFILE_NAME],
        async newProfileName => {
          let newProfileCard =
            content.document.querySelector("new-profile-card").wrappedJSObject;

          await ContentTaskUtils.waitForCondition(
            () => newProfileCard.initialized,
            "Waiting for new-profile-card to be initialized"
          );

          await newProfileCard.getUpdateComplete();

          Assert.equal(
            Services.focus.focusedElement.id,
            newProfileCard.nameInput.id,
            "Name input is focused"
          );

          let nameInput = newProfileCard.nameInput;
          Assert.equal(nameInput.value, "", "Profile name is empty to start");

          nameInput.value = newProfileName;
          nameInput.dispatchEvent(new content.Event("input"));

          await ContentTaskUtils.waitForCondition(() => {
            let savedMessage =
              newProfileCard.shadowRoot.querySelector("#saved-message");
            return ContentTaskUtils.isVisible(savedMessage);
          });
        }
      );

      let curProfile = await SelectableProfileService.getProfile(profile.id);

      Assert.equal(
        curProfile.name,
        NEW_PROFILE_NAME,
        "Profile name was updated in database"
      );

      Assert.equal(
        SelectableProfileService.currentProfile.name,
        NEW_PROFILE_NAME,
        "Current profile name was updated"
      );

      await assertGlean("profiles", "new", "name");
    }
  );

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_new_profile_avatar() {
  if (!AppConstants.MOZ_SELECTABLE_PROFILES) {
    // `mochitest-browser` suite `add_task` does not yet support
    // `properties.skip_if`.
    ok(true, "Skipping because !AppConstants.MOZ_SELECTABLE_PROFILES");
    return;
  }

  let profile = await setup();

  let expectedAvatar = "briefcase";
  // Before we load the new page, set the profile's avatar to something other
  // than the expected item.
  profile.avatar = "flower";
  await SelectableProfileService.updateProfile(profile);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:newprofile",
    },
    async browser => {
      await SpecialPowers.spawn(browser, [expectedAvatar], async expected => {
        let newProfileCard =
          content.document.querySelector("new-profile-card").wrappedJSObject;

        await ContentTaskUtils.waitForCondition(
          () => newProfileCard.initialized,
          "Waiting for new-profile-card to be initialized"
        );

        await newProfileCard.updateComplete;

        // Fill in the input so we don't hit the beforeunload warning
        newProfileCard.nameInput.value = "test";

        let avatars = newProfileCard.avatars;
        let newAvatar = Array.from(avatars).find(
          avatar => avatar.value === expected
        );
        Assert.ok(
          !newAvatar.checked,
          "The new avatar should not initially be selected"
        );
        EventUtils.synthesizeMouseAtCenter(newAvatar, {}, content);

        await ContentTaskUtils.waitForCondition(
          () => newAvatar.checked,
          "Waiting for new avatar to be selected"
        );

        // Sometimes the async message takes a bit longer to arrive.
        await new Promise(resolve => content.setTimeout(resolve, 100));
      });

      let curProfile = await SelectableProfileService.getProfile(profile.id);

      Assert.equal(
        curProfile.avatar,
        expectedAvatar,
        "Profile avatar was updated in database"
      );

      Assert.equal(
        SelectableProfileService.currentProfile.avatar,
        expectedAvatar,
        "Current profile avatar was updated"
      );

      await assertGlean("profiles", "new", "avatar", expectedAvatar);
    }
  );
});

add_task(async function test_new_profile_theme() {
  if (!AppConstants.MOZ_SELECTABLE_PROFILES) {
    // `mochitest-browser` suite `add_task` does not yet support
    // `properties.skip_if`.
    ok(true, "Skipping because !AppConstants.MOZ_SELECTABLE_PROFILES");
    return;
  }
  let profile = await setup();

  // Set the profile to the built-in light theme to avoid theme randomization
  // by the new profile card and make the built-in dark theme card available
  // to be clicked.
  SelectableProfileService.currentProfile.theme = {
    themeId: "firefox-compact-light@mozilla.org",
    themeFg: "rgb(21,20,26)",
    themeBg: "#f9f9fb",
  };
  await SelectableProfileService.updateProfile(
    SelectableProfileService.currentProfile
  );
  let lightTheme = await lazy.AddonManager.getAddonByID(
    "firefox-compact-light@mozilla.org"
  );
  await lightTheme.enable();

  let expectedThemeId = "firefox-compact-dark@mozilla.org";

  is(
    null,
    Glean.profilesNew.theme.testGetValue(),
    "We have not recorded any Glean data yet"
  );

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:newprofile",
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        let newProfileCard =
          content.document.querySelector("new-profile-card").wrappedJSObject;

        await ContentTaskUtils.waitForCondition(
          () => newProfileCard.initialized,
          "Waiting for new-profile-card to be initialized"
        );

        await newProfileCard.updateComplete;

        // Fill in the input so we don't hit the beforeunload warning
        newProfileCard.nameInput.value = "test";

        let darkThemeCard = newProfileCard.themeCards[5];

        Assert.ok(
          !darkThemeCard.checked,
          "Dark theme chip should not be selected"
        );
        EventUtils.synthesizeMouseAtCenter(darkThemeCard, {}, content);

        await newProfileCard.updateComplete;
        await ContentTaskUtils.waitForCondition(
          () => darkThemeCard.checked,
          "Waiting for the new theme chip to be selected"
        );

        // Sometimes the theme takes a little longer to update.
        await new Promise(resolve => content.setTimeout(resolve, 100));
      });

      let curProfile = await SelectableProfileService.getProfile(profile.id);

      Assert.equal(
        curProfile.theme.themeId,
        expectedThemeId,
        "Profile theme was updated in database"
      );

      Assert.equal(
        SelectableProfileService.currentProfile.theme.themeId,
        expectedThemeId,
        "Current profile theme was updated"
      );

      await assertGlean("profiles", "new", "theme", expectedThemeId);
    }
  );

  // Restore the light theme for later tests.
  SelectableProfileService.currentProfile.theme = {
    themeId: "firefox-compact-light@mozilla.org",
    themeFg: "rgb(21,20,26)",
    themeBg: "#f9f9fb",
  };
  await SelectableProfileService.updateProfile(
    SelectableProfileService.currentProfile
  );
  lightTheme = await lazy.AddonManager.getAddonByID(
    "firefox-compact-light@mozilla.org"
  );
  await lightTheme.enable();
});

add_task(async function test_new_profile_explore_more_themes() {
  if (!AppConstants.MOZ_SELECTABLE_PROFILES) {
    // `mochitest-browser` suite `add_task` does not yet support
    // `properties.skip_if`.
    ok(true, "Skipping because !AppConstants.MOZ_SELECTABLE_PROFILES");
    return;
  }
  await setup();
  is(
    null,
    Glean.profilesNew.learnMore.testGetValue(),
    "We have not recorded any Glean data yet"
  );

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:newprofile",
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        let newProfileCard =
          content.document.querySelector("new-profile-card").wrappedJSObject;

        await ContentTaskUtils.waitForCondition(
          () => newProfileCard.initialized,
          "Waiting for new-profile-card to be initialized"
        );

        await newProfileCard.updateComplete;

        // Fill in the input so we don't hit the beforeunload warning
        newProfileCard.nameInput.value = "test";

        // To simplify the test, deactivate the link before clicking.
        newProfileCard.moreThemesLink.href = "#";
        newProfileCard.moreThemesLink.target = "";
        EventUtils.synthesizeMouseAtCenter(
          newProfileCard.moreThemesLink,
          {},
          content
        );

        // Wait a turn for the event to propagate.
        await new Promise(resolve => content.setTimeout(resolve, 0));
      });

      await assertGlean("profiles", "new", "learn_more");
    }
  );
});

add_task(async function test_new_profile_displayed_closed_telemetry() {
  if (!AppConstants.MOZ_SELECTABLE_PROFILES) {
    // `mochitest-browser` suite `add_task` does not yet support
    // `properties.skip_if`.
    ok(true, "Skipping because !AppConstants.MOZ_SELECTABLE_PROFILES");
    return;
  }
  await setup();
  is(
    null,
    Glean.profilesNew.displayed.testGetValue(),
    "We have not recorded any Glean data yet"
  );
  is(
    null,
    Glean.profilesNew.closed.testGetValue(),
    "We have not recorded any Glean data yet"
  );

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:newprofile",
    },
    async browser => {
      await assertGlean("profiles", "new", "displayed");

      Services.fog.testResetFOG();
      Services.telemetry.clearEvents();

      await SpecialPowers.spawn(browser, [], async () => {
        let newProfileCard =
          content.document.querySelector("new-profile-card").wrappedJSObject;

        await ContentTaskUtils.waitForCondition(
          () => newProfileCard.initialized,
          "Waiting for new-profile-card to be initialized"
        );

        await newProfileCard.updateComplete;

        let nameInput = newProfileCard.nameInput;
        nameInput.value = "test profile name";
        nameInput.dispatchEvent(new content.Event("input"));

        await ContentTaskUtils.waitForCondition(() => {
          let savedMessage =
            newProfileCard.shadowRoot.querySelector("#saved-message");
          return ContentTaskUtils.isVisible(savedMessage);
        });

        // Click the done editing button to trigger closed event.
        EventUtils.synthesizeMouseAtCenter(
          newProfileCard.doneButton,
          {},
          content
        );
      });

      await assertGlean("profiles", "new", "closed");
    }
  );
});
