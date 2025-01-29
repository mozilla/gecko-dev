/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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
  await SpecialPowers.pushPrefEnv({
    set: [["dom.require_user_interaction_for_beforeunload", false]],
  });
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
        deleteButton.click();
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

          Assert.equal(
            Services.focus.focusedElement.id,
            editProfileCard.nameInput.id,
            "Name input is focused"
          );

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
  profile.avatar = "flower";
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
          !newAvatar.selected,
          "The new avatar should not initially be selected"
        );
        newAvatar.click();

        await ContentTaskUtils.waitForCondition(
          () => newAvatar.selected,
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
        darkThemeCard.click();

        await ContentTaskUtils.waitForCondition(
          () => darkThemeCard.selected,
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

      // Restore the light theme for later tests.
      curProfile.theme = {
        themeId: "firefox-compact-light@mozilla.org",
        themeFg: "rgb(21,20,26)",
        themeBg: "#f9f9fb",
      };
      await SelectableProfileService.updateProfile(curProfile);
      let profilesParent =
        browser.browsingContext.currentWindowGlobal.getActor("Profiles");
      await profilesParent.enableTheme("firefox-compact-light@mozilla.org", {
        method: "url",
        source: "about:editprofile",
      });
    }
  );
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
        editProfileCard.moreThemesLink.click();

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
        editProfileCard.doneButton.click();
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
        avatars[0].click();
        avatars[0].shadowRoot.querySelector("button").focus();
        let selectedAvatar = editProfileCard.shadowRoot.querySelector(
          "profiles-avatar[selected]"
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
        Assert.ok(!nextAvatar.selected, "The next avatar is not selected");

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
        Assert.ok(!nextAvatar.selected, "The next avatar is not selected");

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
