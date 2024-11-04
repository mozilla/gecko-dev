/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(initSelectableProfileService);

add_task(
  {
    skip_if: () => !AppConstants.MOZ_SELECTABLE_PROFILES,
  },
  async function test_launcher() {
    // mock() returns an object with a fake `runw` method that, when
    // called, records its arguments.
    let input = [];
    let mock = () => {
      return {
        runw: (...args) => {
          input = args;
        },
      };
    };

    let profile = await createTestProfile();

    const SelectableProfileService = getSelectableProfileService();
    SelectableProfileService.getExecutableProcess = mock;
    SelectableProfileService.launchInstance(profile);

    let expected;
    if (Services.appinfo.OS == "Darwin") {
      expected = ["-foreground", "--profile", profile.path];
    } else {
      expected = ["--profile", profile.path];
    }

    Assert.deepEqual(expected, input[1], "Expected runw arguments");
  }
);
