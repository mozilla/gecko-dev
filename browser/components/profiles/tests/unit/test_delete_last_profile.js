/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(initSelectableProfileService);

add_task(async function test_delete_last_profile() {
  // mock() returns an object with a fake `runw` method that, when
  // called, records its arguments.
  let input = [];
  let mock = args => {
    input = args;
  };

  const SelectableProfileService = getSelectableProfileService();
  SelectableProfileService.execProcess = mock;

  let profiles = await SelectableProfileService.getAllProfiles();
  Assert.equal(profiles.length, 1, "Only 1 profile exists before deleting");

  let profile = profiles[0];
  Assert.equal(
    SelectableProfileService.groupToolkitProfile.rootDir.path,
    profile.path,
    "The group toolkit profile path should be the path of the original profile"
  );

  await SelectableProfileService.setShowProfileSelectorWindow(true);
  Assert.ok(
    SelectableProfileService.groupToolkitProfile.showProfileSelector,
    "Show profile selector is enabled"
  );

  let updated = updateNotified();
  await SelectableProfileService.deleteCurrentProfile();
  await updated;

  profiles = await SelectableProfileService.getAllProfiles();
  Assert.equal(profiles.length, 1, "Only 1 profile exists after deleting");

  profile = profiles[0];

  let expectedRunwArgs;
  if (Services.appinfo.OS == "Darwin") {
    expectedRunwArgs = [
      "-foreground",
      "--profile",
      profile.path,
      "-url",
      "about:newprofile",
    ];
  } else {
    expectedRunwArgs = ["--profile", profile.path, "-url", "about:newprofile"];
  }
  Assert.deepEqual(expectedRunwArgs, input, "Expected runw arguments");

  Assert.equal(
    SelectableProfileService.groupToolkitProfile.rootDir.path,
    profile.path,
    "The group toolkit profile path should be the path of the newly created profile"
  );

  Assert.ok(
    !SelectableProfileService.groupToolkitProfile.showProfileSelector,
    "Show profile selector is disabled"
  );
});
