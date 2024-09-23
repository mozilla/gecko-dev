/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(
  {
    skip_if: () => !AppConstants.MOZ_SELECTABLE_PROFILES,
  },
  async function test_SelectableProfileLifecycle() {
    const { SelectableProfileService } = ChromeUtils.importESModule(
      "resource:///modules/profiles/SelectableProfileService.sys.mjs"
    );

    await SelectableProfileService.init();

    let profiles = await SelectableProfileService.getAllProfiles();

    Assert.ok(!profiles.length, "No selectable profiles exist yet");

    let createdProfile = await SelectableProfileService.createProfile({
      name: "testProfile",
      avatar: "avatar",
      themeL10nId: "theme-id",
      themeFg: "redFG",
      themeBg: "blueBG",
    });

    const leafName = (await createdProfile.rootDir).leafName;

    const profilePath = PathUtils.join(
      Services.dirsvc.get("DefProfRt", Ci.nsIFile).path,
      leafName
    );

    let profileDirExists = await IOUtils.exists(profilePath);
    const profileLocalPath = PathUtils.join(
      Services.dirsvc.get("DefProfLRt", Ci.nsIFile).path,
      leafName
    );
    let profileLocalDirExists = await IOUtils.exists(profileLocalPath);

    Assert.ok(
      profileDirExists,
      `Profile dir was successfully created at ${profilePath}`
    );
    Assert.ok(
      profileLocalDirExists,
      `Profile local dir was successfully created at ${profileLocalPath}`
    );

    profiles = await SelectableProfileService.getAllProfiles();

    Assert.equal(profiles.length, 1, "One selectable profile exists");

    let selectableProfile = profiles[0];

    let profile = await SelectableProfileService.getProfile(
      selectableProfile.id
    );

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

    await SelectableProfileService.updateProfile(selectableProfile);

    profile = await SelectableProfileService.getProfile(selectableProfile.id);

    Assert.equal(
      profile.name,
      "updatedTestProfile",
      "We got the correct profile name: updatedTestProfile"
    );

    await SelectableProfileService.deleteProfile(selectableProfile, true);

    profileDirExists = await IOUtils.exists(
      PathUtils.join(
        Services.dirsvc.get("DefProfRt", Ci.nsIFile).path,
        leafName
      )
    );
    profileLocalDirExists = await IOUtils.exists(
      PathUtils.join(
        Services.dirsvc.get("DefProfLRt", Ci.nsIFile).path,
        leafName
      )
    );
    Assert.ok(!profileDirExists, "Profile dir was successfully removed");
    Assert.ok(
      !profileLocalDirExists,
      "Profile local dir was successfully removed"
    );

    profiles = await SelectableProfileService.getAllProfiles();

    Assert.ok(!profiles.length, "No selectable profiles exist yet");

    await SelectableProfileService.deleteProfileGroup();
  }
);
