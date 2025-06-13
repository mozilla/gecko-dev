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
        await new Promise(r => content.setTimeout(r, 4000));
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
