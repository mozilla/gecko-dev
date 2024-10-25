/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(initSelectableProfileService);

add_task(
  {
    skip_if: () => !AppConstants.MOZ_SELECTABLE_PROFILES,
  },
  async function test_SharedPrefsLifecycle() {
    const SelectableProfileService = getSelectableProfileService();
    let prefs = await SelectableProfileService.getAllPrefs();

    Assert.equal(prefs.length, 3, "The default shared prefs exist");

    await SelectableProfileService.setIntPref("testPrefInt0", 0);
    await SelectableProfileService.setIntPref("testPrefInt1", 1);
    await SelectableProfileService.setPref("testPrefInt2", 2);

    await SelectableProfileService.setStringPref(
      "testPrefString0",
      "Hello world!"
    );
    await SelectableProfileService.setPref("testPrefString1", "Hello world 2!");

    await SelectableProfileService.setBoolPref("testPrefBoolTrue", true);
    await SelectableProfileService.setPref("testPrefBoolFalse", false);

    prefs = await SelectableProfileService.getAllPrefs();

    Assert.equal(prefs.length, 10, "10 shared prefs exist");

    Assert.equal(
      await SelectableProfileService.getIntPref("testPrefInt0"),
      0,
      "testPrefInt0 value is 0"
    );
    Assert.equal(
      await SelectableProfileService.getIntPref("testPrefInt1"),
      1,
      "testPrefInt1 value is 1"
    );
    Assert.equal(
      await SelectableProfileService.getPref("testPrefInt2"),
      2,
      "testPrefInt2 value is 2"
    );
    Assert.equal(
      await SelectableProfileService.getStringPref("testPrefString0"),
      "Hello world!",
      'testPrefString0 value is "Hello world!"'
    );
    Assert.equal(
      await SelectableProfileService.getPref("testPrefString1"),
      "Hello world 2!",
      'testPrefString1 value is "Hello world 2!"'
    );
    Assert.equal(
      await SelectableProfileService.getBoolPref("testPrefBoolTrue"),
      true,
      "testPrefBoolTrue value is true"
    );
    Assert.equal(
      await SelectableProfileService.getPref("testPrefBoolFalse"),
      false,
      "testPrefBoolFalse value is false"
    );

    await SelectableProfileService.deleteProfileGroup();
  }
);
