/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_dbLazilyCreated() {
  if (!AppConstants.MOZ_SELECTABLE_PROFILES) {
    // `mochitest-browser` suite `add_task` does not yet support
    // `properties.skip_if`.
    ok(true, "Skipping because !AppConstants.MOZ_SELECTABLE_PROFILES");
    return;
  }

  Assert.ok(
    !SelectableProfileService.initialized,
    `Selectable Profile Service should not be initialized because the default profile has no storeID`
  );

  // Mock the executable process so we doon't launch a new process
  SelectableProfileService.getExecutableProcess = () => {
    return { runw: () => {} };
  };

  await SelectableProfileService.createNewProfile();
  ok(
    SelectableProfileService.initialized,
    `Selectable Profile Service should be initialized because the store id is ${SelectableProfileService.groupToolkitProfile.storeID}`
  );
});
