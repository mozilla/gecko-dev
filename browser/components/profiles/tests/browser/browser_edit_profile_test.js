/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const NEW_PROFILE_NAME = "This is a new profile name";

add_task(async function test_edit_profile_name() {
  if (!AppConstants.MOZ_SELECTABLE_PROFILES) {
    // `mochitest-browser` suite `add_task` does not yet support
    // `properties.skip_if`.
    ok(true, "Skipping because !AppConstants.MOZ_SELECTABLE_PROFILES");
    return;
  }

  let profile = await setupMockDB();
  let rootDir = await profile.rootDir;

  const toolkitProfileObject = { storeID, rootDir };
  SelectableProfileService.groupToolkitProfile = toolkitProfileObject;

  // re-initialize because we updated the rootDir
  await SelectableProfileService.uninit();
  await SelectableProfileService.init();

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

          await editProfileCard.updateComplete;
          await editProfileCard.mozCard.updateComplete;

          let nameInput = editProfileCard.nameInput;
          nameInput.value = newProfileName;
          nameInput.dispatchEvent(new content.Event("change"));
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
    }
  );
});
