"use strict";

const { AboutWelcomeTelemetry } = ChromeUtils.importESModule(
  "resource:///modules/aboutwelcome/AboutWelcomeTelemetry.sys.mjs"
);
const { AWToolbarButton } = ChromeUtils.importESModule(
  "resource:///modules/aboutwelcome/AWToolbarUtils.sys.mjs"
);

const TOOLBAR_PREF = "browser.aboutwelcome.toolbarButtonEnabled";
const DID_SEE_FINAL_SCREEN_PREF = "browser.aboutwelcome.didSeeFinalScreen";

async function openNewTab() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:newtab",
    false
  );

  registerCleanupFunction(async () => {
    BrowserTestUtils.removeTab(tab);
    await SpecialPowers.popPrefEnv();
  });

  return tab.linkedBrowser;
}

async function assertTelemetryScalars(expectedScalars) {
  let processScalars =
    Services.telemetry.getSnapshotForKeyedScalars("main", true)?.parent ?? {};
  let expectedKeys = Object.keys(expectedScalars);

  //key is something like "browser.ui.customized_widgets"
  for (const key of expectedKeys) {
    const expectedEvents = expectedScalars[key];
    const actualEvents = processScalars[key];

    for (const eventKey of Object.keys(expectedEvents)) {
      Assert.equal(
        expectedEvents[eventKey],
        actualEvents[eventKey],
        `Expected to see the correct value for scalar ${eventKey}, got ${actualEvents[eventKey]}`
      );
    }
  }
}

add_task(async function test_add_and_destroy_toolbar_button() {
  // Clear the final screen pref, which may have been set by other tests
  await SpecialPowers.pushPrefEnv({
    set: [[DID_SEE_FINAL_SCREEN_PREF, false]],
  });
  // Open newtab
  let win = await BrowserTestUtils.openNewBrowserWindow();
  win.BrowserCommands.openTab();
  ok(win, "browser exists");
  // Try to add the button. It shouldn't add because the pref is false
  await AWToolbarButton.maybeAddSetupButton();
  ok(
    !win.document.getElementById("aboutwelcome-button"),
    "Button should not exist"
  );
  // Set the pref and try again
  await SpecialPowers.pushPrefEnv({
    set: [[TOOLBAR_PREF, true]],
  });
  await AWToolbarButton.maybeAddSetupButton();
  // The button should exist
  ok(
    win.document.getElementById("aboutwelcome-button"),
    "Button should be added."
  );

  // Check Keyed Scalars for Telemetry
  const expectedScalarsCreate = {
    "aboutwelcome-button_add_na_bookmarks-bar_create": 1,
  };

  assertTelemetryScalars(expectedScalarsCreate);

  // Switch the pref to false and check again
  await SpecialPowers.pushPrefEnv({
    set: [[TOOLBAR_PREF, false]],
  });
  ok(
    !win.document.getElementById("aboutwelcome-button"),
    "Button should be removed"
  );

  // Check Keyed Scalars for Telemetry
  const expectedScalarsDestroy = {
    "aboutwelcome-button_remove_bookmarks-bar_na_destroy": 1,
  };

  assertTelemetryScalars(expectedScalarsDestroy);

  // Cleanup
  await SpecialPowers.popPrefEnv();
  await BrowserTestUtils.closeWindow(win);
});
