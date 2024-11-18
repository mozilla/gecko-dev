/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function promiseAppMenuOpened() {
  let promiseViewShown = BrowserTestUtils.waitForEvent(
    PanelUI.panel,
    "ViewShown"
  );
  PanelUI.show();
  return promiseViewShown;
}

add_task(async function test_appmenu_updates_on_edit() {
  // Mock the executable process so we don't launch a new process when we
  // create new profiles.
  SelectableProfileService._getExecutableProcess =
    SelectableProfileService.getExecutableProcess;
  registerCleanupFunction(() => {
    SelectableProfileService.getExecutableProcess =
      SelectableProfileService._getExecutableProcess;
  });
  SelectableProfileService.getExecutableProcess = () => {
    return { runw: () => {} };
  };

  // We need to create a second profile for the name to be shown in the app
  // menu.
  await SelectableProfileService.createNewProfile();

  const INITIAL_NAME = "Initial name";
  const UPDATED_NAME = "Updated";

  SelectableProfileService.currentProfile.name = INITIAL_NAME;
  await promiseAppMenuOpened();
  let view = PanelMultiView.getViewNode(document, "appMenu-profiles-button");
  Assert.equal(view.label, INITIAL_NAME, "expected the initial name");

  SelectableProfileService.currentProfile.name = UPDATED_NAME;
  PanelUI.hide();
  await promiseAppMenuOpened();
  Assert.equal(view.label, UPDATED_NAME, "expected the name to be updated");
  PanelUI.hide();
});
