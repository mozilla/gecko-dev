/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const setup = async () => {
  await initGroupDatabase();
  let profile = SelectableProfileService.currentProfile;
  Assert.ok(profile, "Should have a profile now");

  return profile;
};

add_task(async function test_edit_profile_custom_avatar() {
  if (!AppConstants.MOZ_SELECTABLE_PROFILES) {
    // `mochitest-browser` suite `add_task` does not yet support
    // `properties.skip_if`.
    ok(true, "Skipping because !AppConstants.MOZ_SELECTABLE_PROFILES");
    return;
  }
  await setup();

  await SpecialPowers.pushPrefEnv({
    set: [["browser.profiles.updated-avatar-selector", true]],
  });

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

        EventUtils.synthesizeMouseAtCenter(
          editProfileCard.avatarSelectorLink,
          {},
          content
        );

        Assert.ok(
          ContentTaskUtils.isVisible(editProfileCard.avatarSelector),
          "Should be showing the profile avatar selector"
        );
      });
    }
  );

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_edit_profile_custom_avatar_upload() {
  if (!AppConstants.MOZ_SELECTABLE_PROFILES) {
    // `mochitest-browser` suite `add_task` does not yet support
    // `properties.skip_if`.
    ok(true, "Skipping because !AppConstants.MOZ_SELECTABLE_PROFILES");
    return;
  }
  const profile = await setup();

  await SpecialPowers.pushPrefEnv({
    set: [["browser.profiles.updated-avatar-selector", true]],
  });

  const mockAvatarFilePath = await IOUtils.createUniqueFile(
    PathUtils.tempDir,
    "avatar.png"
  );
  const mockAvatarFile = Cc["@mozilla.org/file/local;1"].createInstance(
    Ci.nsIFile
  );
  mockAvatarFile.initWithPath(mockAvatarFilePath);

  const MockFilePicker = SpecialPowers.MockFilePicker;
  MockFilePicker.init(window.browsingContext);
  MockFilePicker.setFiles([mockAvatarFile]);
  MockFilePicker.returnValue = MockFilePicker.returnOK;

  let curProfile = await SelectableProfileService.getProfile(profile.id);
  Assert.ok(
    !curProfile.hasCustomAvatar,
    "Current profile does not have a custom avatar"
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

        const avatarSelector = editProfileCard.avatarSelector;

        EventUtils.synthesizeMouseAtCenter(
          editProfileCard.avatarSelectorLink,
          {},
          content
        );

        Assert.ok(
          ContentTaskUtils.isVisible(avatarSelector),
          "Should be showing the profile avatar selector"
        );

        avatarSelector.state = "custom";
        await avatarSelector.updateComplete;

        await ContentTaskUtils.waitForCondition(
          () => ContentTaskUtils.isVisible(avatarSelector.input),
          "Waiting for avatar selector input to be visible"
        );

        const inputReceived = new Promise(resolve =>
          avatarSelector.input.addEventListener(
            "input",
            event => {
              resolve(event.target.files[0].name);
            },
            { once: true }
          )
        );

        EventUtils.synthesizeMouseAtCenter(avatarSelector.input, {}, content);

        await inputReceived;

        await ContentTaskUtils.waitForCondition(
          () => ContentTaskUtils.isVisible(avatarSelector.saveButton),
          "Waiting for avatar selector save button to be visible"
        );

        EventUtils.synthesizeMouseAtCenter(
          avatarSelector.saveButton,
          {},
          content
        );

        // Sometimes the async message takes a bit longer to arrive.
        await new Promise(resolve => content.setTimeout(resolve, 100));
      });
    }
  );

  curProfile = await SelectableProfileService.getProfile(profile.id);
  Assert.ok(
    curProfile.hasCustomAvatar,
    "Current profile has a custom avatar image"
  );

  MockFilePicker.cleanup();

  await SpecialPowers.popPrefEnv();
});
