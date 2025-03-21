/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { CustomizableUITestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/CustomizableUITestUtils.sys.mjs"
);

async function promiseFocus(win) {
  if (!win.document.hasFocus()) {
    await BrowserTestUtils.waitForEvent(win.document, "focus");
  }
}

add_task(async function test_selector_window() {
  await initGroupDatabase();
  let profile = SelectableProfileService.currentProfile;

  let cuiTestUtils = new CustomizableUITestUtils(window);

  await cuiTestUtils.openMainMenu();

  let profilesButton = PanelMultiView.getViewNode(
    document,
    "appMenu-profiles-button"
  );
  let subview = PanelMultiView.getViewNode(document, "PanelUI-profiles");

  let viewShown = BrowserTestUtils.waitForEvent(subview, "ViewShown");
  profilesButton.click();
  await viewShown;

  let manageButton = PanelMultiView.getViewNode(
    document,
    "profiles-manage-profiles-button"
  );

  let windowOpened = BrowserTestUtils.domWindowOpenedAndLoaded();

  await cuiTestUtils.hidePanelMultiView(cuiTestUtils.PanelUI.panel, () =>
    manageButton.click()
  );

  let dialog = await windowOpened;
  await promiseFocus(dialog);
  Assert.equal(dialog.location, "about:profilemanager");

  let deactivated = BrowserTestUtils.waitForEvent(dialog, "deactivate");
  await SimpleTest.promiseFocus(window);
  await deactivated;

  await cuiTestUtils.openMainMenu();
  viewShown = BrowserTestUtils.waitForEvent(subview, "ViewShown");
  profilesButton.click();
  await viewShown;

  let activated = promiseFocus(dialog);
  await cuiTestUtils.hidePanelMultiView(cuiTestUtils.PanelUI.panel, () =>
    manageButton.click()
  );
  await activated;

  let closed = BrowserTestUtils.domWindowClosed(dialog);

  // mock() returns an object with a fake `runw` method that, when
  // called, records its arguments.
  let input = [];
  let mock = args => (input = args);

  const profileSelector = dialog.document.querySelector("profile-selector");
  await profileSelector.updateComplete;

  Assert.ok(profileSelector.checkbox.checked, "Checkbox should be checked");

  let asyncFlushCalled = false;
  gProfileService.asyncFlush = () => (asyncFlushCalled = true);

  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();
  Services.telemetry.clearEvents();
  is(
    null,
    Glean.profilesSelectorWindow.showAtStartup.testGetValue(),
    "We have not recorded any Glean data yet"
  );

  profileSelector.checkbox.click();
  await BrowserTestUtils.waitForCondition(
    () => asyncFlushCalled,
    "Expected asyncFlush to be called"
  );
  asyncFlushCalled = false;

  Assert.ok(
    !profileSelector.checkbox.checked,
    "Checkbox should not be checked"
  );
  Assert.ok(
    !gProfileService.groupProfile.showProfileSelector,
    "Profile selector should be disabled"
  );

  await assertGlean(
    "profiles",
    "selector_window",
    "show_at_startup",
    "disabled"
  );

  // Simulate matching state.
  gProfileService.groupProfile.showProfileSelector = true;

  profileSelector.checkbox.click();
  await BrowserTestUtils.waitForCondition(
    () => asyncFlushCalled,
    "Expected asyncFlush to be called"
  );
  asyncFlushCalled = false;

  Assert.ok(profileSelector.checkbox.checked, "Checkbox should not be checked");
  Assert.ok(
    gProfileService.groupProfile.showProfileSelector,
    "Profile selector should be disabled"
  );

  profileSelector.selectableProfileService.execProcess = mock;

  const profiles = profileSelector.profileCards;

  Assert.equal(profiles.length, 1, "There is one profile card");
  Assert.ok(
    profileSelector.createProfileCard,
    "The create profile card exists"
  );

  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();
  Services.telemetry.clearEvents();
  is(
    null,
    Glean.profilesSelectorWindow.launch.testGetValue(),
    "We have not recorded any Glean data yet"
  );

  profileSelector.profileCards[0].click();

  let expected;
  if (Services.appinfo.OS === "Darwin") {
    expected = [
      "-foreground",
      "--profile",
      profile.path,
      "--profiles-activate",
    ];
  } else {
    expected = ["--profile", profile.path, "--profiles-activate"];
  }

  Assert.deepEqual(input, expected, "Expected runw arguments");

  await assertGlean("profiles", "selector_window", "launch");

  await closed;
});
