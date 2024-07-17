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
  async function test_SharedPrefsLifecycle() {
    let sps = new SelectableProfileService();
    await sps.init();

    let prefs = await sps.getAllPrefs();

    Assert.ok(!prefs.length, "No shared prefs exist yet");

    await sps.setIntPref("testPrefInt0", 0);
    await sps.setIntPref("testPrefInt1", 1);
    await sps.setPref("testPrefInt2", 2);

    await sps.setStringPref("testPrefString0", "Hello world!");
    await sps.setPref("testPrefString1", "Hello world 2!");

    await sps.setBoolPref("testPrefBoolTrue", true);
    await sps.setPref("testPrefBoolFalse", false);

    prefs = await sps.getAllPrefs();

    Assert.equal(prefs.length, 7, "7 shared prefs exist");

    Assert.equal(
      await sps.getIntPref("testPrefInt0"),
      0,
      "testPrefInt0 value is 0"
    );
    Assert.equal(
      await sps.getIntPref("testPrefInt1"),
      1,
      "testPrefInt1 value is 1"
    );
    Assert.equal(
      await sps.getPref("testPrefInt2"),
      2,
      "testPrefInt2 value is 2"
    );
    Assert.equal(
      await sps.getStringPref("testPrefString0"),
      "Hello world!",
      'testPrefString0 value is "Hello world!"'
    );
    Assert.equal(
      await sps.getPref("testPrefString1"),
      "Hello world 2!",
      'testPrefString1 value is "Hello world 2!"'
    );
    Assert.equal(
      await sps.getBoolPref("testPrefBoolTrue"),
      true,
      "testPrefBoolTrue value is true"
    );
    Assert.equal(
      await sps.getPref("testPrefBoolFalse"),
      false,
      "testPrefBoolFalse value is false"
    );

    await sps.deleteProfileGroup();
  }
);
