/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);
const { SelectableProfileService } = ChromeUtils.importESModule(
  "resource:///modules/profiles/SelectableProfileService.sys.mjs"
);
const { makeFakeAppDir } = ChromeUtils.importESModule(
  "resource://testing-common/AppData.sys.mjs"
);

add_setup(async () => {
  do_get_profile();
  await makeFakeAppDir();
});

add_task(
  {
    skip_if: () => !AppConstants.MOZ_SELECTABLE_PROFILES,
  },
  async function test_SelectableProfileLifecycle() {
    let sps = new SelectableProfileService();
    await sps.init();

    let profiles = await sps.getProfiles();

    Assert.ok(!profiles.length, "No selectable profiles exist yet");

    let createdProfile = await sps.createProfile({
      name: "testProfile",
      path: "/testPath/testProfile",
      avatar: "avatar",
      themeL10nId: "theme-id",
      themeFg: "redFG",
      themeBg: "blueBG",
    });

    profiles = await sps.getProfiles();

    Assert.equal(profiles.length, 1, "One selectable profile exists");

    let selectableProfile = profiles[0];

    let profile = await sps.getProfile(selectableProfile.id);

    for (let attr of ["id", "name", "path"]) {
      Assert.equal(
        profile[attr],
        createdProfile[attr],
        `We got the correct profile ${attr}`
      );

      Assert.equal(
        selectableProfile[attr],
        createdProfile[attr],
        `We got the correct profile ${attr}`
      );
    }

    selectableProfile.name = "updatedTestProfile";

    await sps.updateProfile(selectableProfile);

    profile = await sps.getProfile(selectableProfile.id);

    Assert.equal(
      profile.name,
      "updatedTestProfile",
      "We got the correct profile name: updatedTestProfile"
    );

    await sps.deleteProfile(selectableProfile);

    profiles = await sps.getProfiles();

    Assert.ok(!profiles.length, "No selectable profiles exist yet");

    await sps.deleteProfileGroup();
  }
);
