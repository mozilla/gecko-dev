/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_recover_storeID() {
  startProfileService();
  Services.prefs.setCharPref("toolkit.profiles.storeID", "foobar");

  const SelectableProfileService = getSelectableProfileService();
  await SelectableProfileService.init();
  Assert.ok(
    !SelectableProfileService.initialized,
    "Didn't initialize the service"
  );

  let profile = SelectableProfileService.currentProfile;
  Assert.ok(!profile, "Should not have a current profile");
  Assert.equal(
    getProfileService().currentProfile.storeID,
    null,
    "Should not have updated the store ID on the profile"
  );

  Assert.ok(
    !Services.prefs.prefHasUserValue("toolkit.profiles.storeID"),
    "Should have cleared the storeID pref"
  );
});
