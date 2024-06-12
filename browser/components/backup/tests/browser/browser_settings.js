/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the section for controlling backup in about:preferences is
 * visible, but can also be hidden via a pref.
 */
add_task(async function test_preferences_visibility() {
  await BrowserTestUtils.withNewTab("about:preferences", async browser => {
    let backupSection =
      browser.contentDocument.querySelector("#dataBackupGroup");
    Assert.ok(backupSection, "Found backup preferences section");

    // Our mochitest-browser tests are configured to have the section visible
    // by default.
    Assert.ok(
      BrowserTestUtils.isVisible(backupSection),
      "Backup section is visible"
    );
  });

  await SpecialPowers.pushPrefEnv({
    set: [["browser.backup.preferences.ui.enabled", false]],
  });

  await BrowserTestUtils.withNewTab("about:preferences", async browser => {
    let backupSection =
      browser.contentDocument.querySelector("#dataBackupGroup");
    Assert.ok(backupSection, "Found backup preferences section");

    Assert.ok(
      BrowserTestUtils.isHidden(backupSection),
      "Backup section is now hidden"
    );
  });

  await SpecialPowers.popPrefEnv();
});

/**
 * Tests that the turn off scheduled backups dialog can set
 * browser.backup.scheduled.enabled to false from the settings page.
 */
add_task(async function test_turn_off_scheduled_backups_confirm() {
  await BrowserTestUtils.withNewTab("about:preferences", async browser => {
    const SCHEDULED_BACKUPS_ENABLED_PREF = "browser.backup.scheduled.enabled";

    await SpecialPowers.pushPrefEnv({
      set: [[SCHEDULED_BACKUPS_ENABLED_PREF, true]],
    });

    let settings = browser.contentDocument.querySelector("backup-settings");

    await settings.updateComplete;

    let turnOffButton = settings.scheduledBackupsButtonEl;

    Assert.ok(
      turnOffButton,
      "Button to turn off scheduled backups should be found"
    );

    turnOffButton.click();

    await settings.updateComplete;

    let turnOffScheduledBackups = settings.turnOffScheduledBackupsEl;

    Assert.ok(
      turnOffScheduledBackups,
      "turn-off-scheduled-backups should be found"
    );

    let confirmButton = turnOffScheduledBackups.confirmButtonEl;
    let promise = BrowserTestUtils.waitForEvent(
      window,
      "turnOffScheduledBackups"
    );

    Assert.ok(confirmButton, "Confirm button should be found");

    confirmButton.click();

    await promise;
    await settings.updateComplete;

    let scheduledPrefVal = Services.prefs.getBoolPref(
      SCHEDULED_BACKUPS_ENABLED_PREF
    );
    Assert.ok(!scheduledPrefVal, "Scheduled backups pref should be false");

    await SpecialPowers.popPrefEnv();
  });
});
