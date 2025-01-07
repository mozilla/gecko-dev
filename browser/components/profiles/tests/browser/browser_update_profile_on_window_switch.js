/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { SelectableProfile } = ChromeUtils.importESModule(
  "resource:///modules/profiles/SelectableProfile.sys.mjs"
);

add_task(async function test_updateDefaultProfileOnWindowSwitch() {
  await initGroupDatabase();
  let currentProfile = SelectableProfileService.currentProfile;
  let profileRootDir = await currentProfile.rootDir;

  ok(
    SelectableProfileService.currentProfile instanceof SelectableProfile,
    "The current selectable profile exists"
  );
  is(
    gProfileService.currentProfile.rootDir.path,
    profileRootDir.path,
    `The SelectableProfileService rootDir is correct`
  );

  Services.telemetry.clearEvents();
  Services.fog.testResetFOG();
  is(
    null,
    Glean.profilesDefault.updated.testGetValue(),
    "We have not recorded any Glean data yet"
  );

  // Override
  gProfileService.currentProfile.rootDir = "bad";

  let w = await BrowserTestUtils.openNewBrowserWindow();
  w.focus();

  let asyncFlushCalled = false;
  gProfileService.asyncFlush = () => (asyncFlushCalled = true);

  // Focus the original window so we get an "activate" event and update the toolkitProfile rootDir
  window.focus();

  await BrowserTestUtils.waitForCondition(
    () => asyncFlushCalled,
    "Expected asyncFlush to be called"
  );

  let unfocused = BrowserTestUtils.waitForEvent(window, "deactivate");
  w.focus();
  await unfocused;

  gProfileService.asyncFlush = () => {
    throw new Error("Failed");
  };
  let asyncFlushGroupProfileCalled = false;
  gProfileService.asyncFlushGroupProfile = () =>
    (asyncFlushGroupProfileCalled = true);

  window.focus();

  await BrowserTestUtils.waitForCondition(
    () => asyncFlushGroupProfileCalled,
    "Expected asyncFlushGroupProfile to be called"
  );

  is(
    gProfileService.currentProfile.rootDir.path,
    profileRootDir.path,
    `The SelectableProfileService rootDir is correct`
  );

  let testEvents = Glean.profilesDefault.updated.testGetValue();
  Assert.equal(
    3,
    testEvents.length,
    "Should have recorded the default profile updated event exactly once"
  );
  TelemetryTestUtils.assertEvents(
    [
      ["profiles", "default", "updated"],
      ["profiles", "default", "updated"],
      ["profiles", "default", "updated"],
    ],
    {
      category: "profiles",
      method: "default",
    }
  );

  await BrowserTestUtils.closeWindow(w);
  await SelectableProfileService.uninit();
});
