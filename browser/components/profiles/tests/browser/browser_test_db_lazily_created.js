/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

add_setup(() => {
  // Mock the executable process so we doon't launch a new process
  sinon.stub(SelectableProfileService, "execProcess");

  registerCleanupFunction(() => {
    sinon.restore();
  });
});

add_task(async function test_dbLazilyCreated() {
  Assert.ok(
    !SelectableProfileService.initialized,
    `Selectable Profile Service should not be initialized because the default profile has no storeID`
  );

  await SelectableProfileService.maybeSetupDataStore();
  ok(
    SelectableProfileService.initialized,
    `Selectable Profile Service should be initialized because the store id is ${SelectableProfileService.groupToolkitProfile.storeID}`
  );

  ok(
    SelectableProfileService.groupToolkitProfile.showProfileSelector,
    "Once the user has created a second profile, ShowSelector should be set to true"
  );
});
