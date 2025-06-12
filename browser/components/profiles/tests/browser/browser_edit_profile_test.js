/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
});

const NEW_PROFILE_NAME = "This is a new profile name";

const setup = async () => {
  await initGroupDatabase();
  let profile = SelectableProfileService.currentProfile;
  Assert.ok(profile, "Should have a profile now");

  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();
  Services.telemetry.clearEvents();
  return profile;
};

add_task(async function test_edit_profile_delete() {
  if (!AppConstants.MOZ_SELECTABLE_PROFILES) {
    // `mochitest-browser` suite `add_task` does not yet support
    // `properties.skip_if`.
    ok(true, "Skipping because !AppConstants.MOZ_SELECTABLE_PROFILES");
    return;
  }
  await setup();
  is(
    null,
    Glean.profilesExisting.deleted.testGetValue(),
    "We have not recorded any Glean data yet"
  );
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:editprofile",
    },
    async browser => {
      let deletePageLoaded = BrowserTestUtils.browserLoaded(
        browser,
        false,
        "about:deleteprofile"
      );

      await SpecialPowers.spawn(browser, [], async () => {
        let editProfileCard =
          content.document.querySelector("edit-profile-card").wrappedJSObject;

        await ContentTaskUtils.waitForCondition(
          () => editProfileCard.initialized,
          "Waiting for edit-profile-card to be initialized"
        );

        await editProfileCard.updateComplete;

        let nameInput = editProfileCard.nameInput;
        nameInput.value = "";
        nameInput.dispatchEvent(new content.Event("input"));

        let deleteButton = editProfileCard.deleteButton;
        EventUtils.synthesizeMouseAtCenter(deleteButton, {}, content);
      });

      await deletePageLoaded;

      await assertGlean("profiles", "existing", "deleted");
    }
  );
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_edit_profile_name() {
  if (!AppConstants.MOZ_SELECTABLE_PROFILES) {
    // `mochitest-browser` suite `add_task` does not yet support
    // `properties.skip_if`.
    ok(true, "Skipping because !AppConstants.MOZ_SELECTABLE_PROFILES");
    return;
  }
  let profile = await setup();
  is(
    null,
    Glean.profilesExisting.name.testGetValue(),
    "We have not recorded any Glean data yet"
  );
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:editprofile",
    },
    async browser => {
      await SpecialPowers.spawn(
        browser,
        [NEW_PROFILE_NAME],
        async newProfileName => {
          let editProfileCard =
            content.document.querySelector("edit-profile-card").wrappedJSObject;

          await ContentTaskUtils.waitForCondition(
            () => editProfileCard.initialized,
            "Waiting for edit-profile-card to be initialized"
          );

          await editProfileCard.getUpdateComplete();

          let nameInput = editProfileCard.nameInput;
          nameInput.value = newProfileName;
          nameInput.dispatchEvent(new content.Event("input"));

          await ContentTaskUtils.waitForCondition(() => {
            let savedMessage =
              editProfileCard.shadowRoot.querySelector("#saved-message");
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

      await assertGlean("profiles", "existing", "name");
    }
  );
});

add_task(async function test_edit_profile_avatar() {
  if (!AppConstants.MOZ_SELECTABLE_PROFILES) {
    // `mochitest-browser` suite `add_task` does not yet support
    // `properties.skip_if`.
    ok(true, "Skipping because !AppConstants.MOZ_SELECTABLE_PROFILES");
    return;
  }
  let profile = await setup();

  // Before we load the edit page, set the profile's avatar to something other
  // than the 0th item.
  await profile.setAvatar("flower");
  let expectedAvatar = "book";

  is(
    null,
    Glean.profilesExisting.avatar.testGetValue(),
    "We have not recorded any Glean data yet"
  );

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:editprofile",
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        let editProfileCard =
          content.document.querySelector("edit-profile-card").wrappedJSObject;

        await ContentTaskUtils.waitForCondition(
          () => editProfileCard.initialized,
          "Waiting for edit-profile-card to be initialized"
        );

        await editProfileCard.updateComplete;

        let avatars = editProfileCard.avatars;
        let newAvatar = avatars[0];
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

      await assertGlean("profiles", "existing", "avatar", expectedAvatar);
    }
  );
});

add_task(async function test_edit_profile_theme() {
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
  let lightTheme = await lazy.AddonManager.getAddonByID(
    "firefox-compact-light@mozilla.org"
  );
  await lightTheme.enable();

  let expectedThemeId = "firefox-compact-dark@mozilla.org";

  is(
    null,
    Glean.profilesExisting.theme.testGetValue(),
    "We have not recorded any Glean data yet"
  );

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:editprofile",
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        let editProfileCard =
          content.document.querySelector("edit-profile-card").wrappedJSObject;

        await ContentTaskUtils.waitForCondition(
          () => editProfileCard.initialized,
          "Waiting for edit-profile-card to be initialized"
        );

        await editProfileCard.updateComplete;

        let darkThemeCard = editProfileCard.themeCards[5];
        EventUtils.synthesizeMouseAtCenter(darkThemeCard, {}, content);

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

      await assertGlean("profiles", "existing", "theme", expectedThemeId);
    }
  );

  // Restore the light theme for later tests.
  lightTheme = await lazy.AddonManager.getAddonByID(
    "firefox-compact-light@mozilla.org"
  );
  await lightTheme.enable();
});

add_task(async function test_edit_profile_explore_more_themes() {
  if (!AppConstants.MOZ_SELECTABLE_PROFILES) {
    // `mochitest-browser` suite `add_task` does not yet support
    // `properties.skip_if`.
    ok(true, "Skipping because !AppConstants.MOZ_SELECTABLE_PROFILES");
    return;
  }
  await setup();
  is(
    null,
    Glean.profilesExisting.learnMore.testGetValue(),
    "We have not recorded any Glean data yet"
  );

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:editprofile",
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        let editProfileCard =
          content.document.querySelector("edit-profile-card").wrappedJSObject;

        await ContentTaskUtils.waitForCondition(
          () => editProfileCard.initialized,
          "Waiting for edit-profile-card to be initialized"
        );

        await editProfileCard.updateComplete;

        // To simplify the test, deactivate the link before clicking.
        editProfileCard.moreThemesLink.href = "#";
        editProfileCard.moreThemesLink.target = "";
        EventUtils.synthesizeMouseAtCenter(
          editProfileCard.moreThemesLink,
          {},
          content
        );

        // Wait a turn for the event to propagate.
        await new Promise(resolve => content.setTimeout(resolve, 0));
      });

      await assertGlean("profiles", "existing", "learn_more");
    }
  );
});

add_task(async function test_edit_profile_displayed_closed_telemetry() {
  if (!AppConstants.MOZ_SELECTABLE_PROFILES) {
    // `mochitest-browser` suite `add_task` does not yet support
    // `properties.skip_if`.
    ok(true, "Skipping because !AppConstants.MOZ_SELECTABLE_PROFILES");
    return;
  }
  await setup();
  is(
    null,
    Glean.profilesExisting.displayed.testGetValue(),
    "We have not recorded any Glean data yet"
  );
  is(
    null,
    Glean.profilesExisting.closed.testGetValue(),
    "We have not recorded any Glean data yet"
  );

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:editprofile",
    },
    async browser => {
      // Once the page has loaded, the displayed event should be available.
      await assertGlean("profiles", "existing", "displayed");

      Services.fog.testResetFOG();
      Services.telemetry.clearEvents();

      await SpecialPowers.spawn(browser, [], async () => {
        let editProfileCard =
          content.document.querySelector("edit-profile-card").wrappedJSObject;

        await ContentTaskUtils.waitForCondition(
          () => editProfileCard.initialized,
          "Waiting for edit-profile-card to be initialized"
        );

        await editProfileCard.updateComplete;

        // Click the done editing button to trigger closed event.
        EventUtils.synthesizeMouseAtCenter(
          editProfileCard.doneButton,
          {},
          content
        );
      });

      await assertGlean("profiles", "existing", "closed");
    }
  );
});

add_task(async function test_avatar_picker_arrow_key_support() {
  if (!AppConstants.MOZ_SELECTABLE_PROFILES) {
    // `mochitest-browser` suite `add_task` does not yet support
    // `properties.skip_if`.
    ok(true, "Skipping because !AppConstants.MOZ_SELECTABLE_PROFILES");
    return;
  }

  await initGroupDatabase();
  let profile = SelectableProfileService.currentProfile;
  Assert.ok(profile, "Should have a profile now");

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:editprofile",
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        const EventUtils = ContentTaskUtils.getEventUtils(content);

        let editProfileCard =
          content.document.querySelector("edit-profile-card").wrappedJSObject;

        await ContentTaskUtils.waitForCondition(
          () => editProfileCard.initialized,
          "Waiting for edit-profile-card to be initialized"
        );
        await editProfileCard.updateComplete;
        let avatars = editProfileCard.avatars;

        // Select and focus the book avatar to get started.
        EventUtils.synthesizeMouseAtCenter(avatars[0], {}, content);
        avatars[0].focus();
        let selectedAvatar = editProfileCard.shadowRoot.querySelector(
          "#avatars > profiles-group-item[checked]"
        );
        Assert.equal("book", selectedAvatar.value, "Book avatar was selected");
        Assert.equal(
          editProfileCard.shadowRoot.activeElement,
          selectedAvatar,
          "The selected avatar has focus"
        );

        // Simulate a down arrow key and the focus should move, making the
        // next element focused, but not selected.
        let nextAvatar = selectedAvatar.nextElementSibling;
        EventUtils.synthesizeKey("KEY_ArrowDown", {}, content);
        Assert.equal(
          editProfileCard.shadowRoot.activeElement,
          nextAvatar,
          "The next avatar has focus"
        );
        Assert.ok(!nextAvatar.checked, "The next avatar is not selected");

        // Now, use the up arrow key to move focus back.
        EventUtils.synthesizeKey("KEY_ArrowUp", {}, content);
        Assert.equal(
          editProfileCard.shadowRoot.activeElement,
          selectedAvatar,
          "The selected avatar has focus"
        );

        // Same thing, this time using the right and left arrows:

        EventUtils.synthesizeKey("KEY_ArrowRight", {}, content);
        Assert.equal(
          editProfileCard.shadowRoot.activeElement,
          nextAvatar,
          "The next avatar has focus"
        );
        Assert.ok(!nextAvatar.checked, "The next avatar is not selected");

        EventUtils.synthesizeKey("KEY_ArrowLeft", {}, content);
        Assert.equal(
          editProfileCard.shadowRoot.activeElement,
          selectedAvatar,
          "The selected avatar has focus"
        );
      });
    }
  );
});

add_task(async function test_theme_picker_arrow_key_support() {
  if (!AppConstants.MOZ_SELECTABLE_PROFILES) {
    // `mochitest-browser` suite `add_task` does not yet support
    // `properties.skip_if`.
    ok(true, "Skipping because !AppConstants.MOZ_SELECTABLE_PROFILES");
    return;
  }

  await initGroupDatabase();
  let profile = SelectableProfileService.currentProfile;
  Assert.ok(profile, "Should have a profile now");

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:editprofile",
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        const EventUtils = ContentTaskUtils.getEventUtils(content);

        let editProfileCard =
          content.document.querySelector("edit-profile-card").wrappedJSObject;

        await ContentTaskUtils.waitForCondition(
          () => editProfileCard.initialized,
          "Waiting for edit-profile-card to be initialized"
        );
        await editProfileCard.updateComplete;
        let themeCards = editProfileCard.themeCards;

        // Select and focus the light theme to get started.
        EventUtils.synthesizeMouseAtCenter(themeCards[0], {}, content);
        themeCards[0].focus();
        let selectedTheme = editProfileCard.shadowRoot.querySelector(
          "#themes > profiles-group-item[checked]"
        );
        Assert.equal(
          "firefox-compact-light@mozilla.org",
          selectedTheme.value,
          "Light theme was selected"
        );
        Assert.equal(
          editProfileCard.shadowRoot.activeElement,
          selectedTheme,
          "The selected theme has focus"
        );

        // Simulate a down arrow key and the focus should move, making the
        // next element focused, but not selected.
        let nextTheme = selectedTheme.nextElementSibling;
        EventUtils.synthesizeKey("KEY_ArrowDown", {}, content);
        Assert.equal(
          editProfileCard.shadowRoot.activeElement,
          nextTheme,
          "The next theme has focus"
        );
        Assert.ok(!nextTheme.checked, "The next theme is not selected");

        // Now, use the up arrow key to move focus back.
        EventUtils.synthesizeKey("KEY_ArrowUp", {}, content);
        Assert.equal(
          editProfileCard.shadowRoot.activeElement,
          selectedTheme,
          "The selected theme has focus"
        );

        // Same thing, this time using the right and left arrows:

        EventUtils.synthesizeKey("KEY_ArrowRight", {}, content);
        Assert.equal(
          editProfileCard.shadowRoot.activeElement,
          nextTheme,
          "The next theme has focus"
        );
        Assert.ok(!nextTheme.checked, "The next theme is not selected");

        EventUtils.synthesizeKey("KEY_ArrowLeft", {}, content);
        Assert.equal(
          editProfileCard.shadowRoot.activeElement,
          selectedTheme,
          "The selected theme has focus"
        );
      });
    }
  );
});

add_task(async function test_edit_profile_system_theme() {
  if (!AppConstants.MOZ_SELECTABLE_PROFILES) {
    // `mochitest-browser` suite `add_task` does not yet support
    // `properties.skip_if`.
    ok(true, "Skipping because !AppConstants.MOZ_SELECTABLE_PROFILES");
    return;
  }
  let profile = await setup();

  let defaultTheme = await lazy.AddonManager.getAddonByID(
    "default-theme@mozilla.org"
  );
  await defaultTheme.enable();

  // Set to light theme so we can select the system theme in the page
  let lightTheme = await lazy.AddonManager.getAddonByID(
    "firefox-compact-light@mozilla.org"
  );
  await lightTheme.enable();

  let expectedThemeId = "default-theme@mozilla.org";

  let themePromise = new Promise(r =>
    window.addEventListener("windowlwthemeupdate", r, {
      once: true,
    })
  );

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:editprofile",
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        let editProfileCard =
          content.document.querySelector("edit-profile-card").wrappedJSObject;

        await ContentTaskUtils.waitForCondition(
          () => editProfileCard.initialized,
          "Waiting for edit-profile-card to be initialized"
        );

        await editProfileCard.updateComplete;

        let defaultThemeCard = editProfileCard.themeCards[9];
        EventUtils.synthesizeMouseAtCenter(defaultThemeCard, {}, content);

        await ContentTaskUtils.waitForCondition(
          () => defaultThemeCard.checked,
          "Waiting for the new theme chip to be selected"
        );

        // Sometimes the theme takes a little longer to update.
        await new Promise(resolve => content.setTimeout(resolve, 100));
      });

      await themePromise;

      let curProfile = await SelectableProfileService.getProfile(profile.id);

      let themeColors = SelectableProfileService.getColorsForDefaultTheme();

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

      Assert.equal(
        SelectableProfileService.currentProfile.theme.themeBg,
        themeColors.themeBg,
        "Theme background color is expected: " + themeColors.themeBg
      );
      Assert.equal(
        SelectableProfileService.currentProfile.theme.themeFg,
        themeColors.themeFg,
        "Theme  color is expected: " + themeColors.themeFg
      );
    }
  );

  // Restore the light theme for later tests.
  lightTheme = await lazy.AddonManager.getAddonByID(
    "firefox-compact-light@mozilla.org"
  );
  await lightTheme.enable();
});
