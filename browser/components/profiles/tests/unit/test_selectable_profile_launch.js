/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(
  {
    skip_if: () => !AppConstants.MOZ_SELECTABLE_PROFILES,
  },
  async function test_launcher() {
    const { SelectableProfileService } = ChromeUtils.importESModule(
      "resource:///modules/profiles/SelectableProfileService.sys.mjs"
    );

    let profile = do_get_profile();
    await SelectableProfileService.init();

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
