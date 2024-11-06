/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const SCHEDULED_BACKUPS_ENABLED_PREF = "browser.backup.scheduled.enabled";

/**
 * Verifies the turn off button and the turn-off dialog in the settings page.
 * Disables scheduled backups by confirming the dialog.
 *
 * @param {object} browser
 *  The browser object in which we load backup-settings and will use for tests
 * @param {Function} taskFn
 *  A function that is run once all default checks are done.
 */
async function turnOffScheduledBackupsHelper(browser, taskFn) {
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
    "BackupUI:DisableScheduledBackups"
  );

  Assert.ok(confirmButton, "Confirm button should be found");

  confirmButton.click();

  await promise;
  await settings.updateComplete;

  await taskFn();
}

/**
 * Tests that the turn off scheduled backups dialog can set
 * browser.backup.scheduled.enabled to false from the settings page
 * and that the most recent backup is deleted once confirmed.
 */
add_task(async function test_turn_off_scheduled_backups_confirm() {
  await BrowserTestUtils.withNewTab("about:preferences", async browser => {
    Services.telemetry.clearEvents();
    Services.fog.testResetFOG();

    let sandbox = sinon.createSandbox();
    let deleteLastBackupStub = sandbox
      .stub(BackupService.prototype, "deleteLastBackup")
      .resolves(true);

    await SpecialPowers.pushPrefEnv({
      set: [[SCHEDULED_BACKUPS_ENABLED_PREF, true]],
    });

    await turnOffScheduledBackupsHelper(browser, () => {
      let scheduledPrefVal = Services.prefs.getBoolPref(
        SCHEDULED_BACKUPS_ENABLED_PREF
      );
      Assert.ok(!scheduledPrefVal, "Scheduled backups pref should be false");

      Assert.ok(
        deleteLastBackupStub.calledOnce,
        "BackupService was called to delete the latest backup file"
      );
    });

    let legacyEvents = TelemetryTestUtils.getEvents(
      {
        category: "browser.backup",
        method: "toggle_off",
        object: "BackupService",
      },
      { process: "parent" }
    );
    Assert.equal(legacyEvents.length, 1, "Found the toggle_off legacy event.");
    let events = Glean.browserBackup.toggleOff.testGetValue();
    Assert.equal(events.length, 1, "Found the toggleOff Glean event.");

    await SpecialPowers.popPrefEnv();
    sandbox.restore();
  });
});

/*
 * Tests that if a backup was encrypted after turning off scheduled backups,
 * encryption is disabled as a result and the latest backup is still deleted.
 */
add_task(async function test_turn_off_scheduled_backups_disables_encryption() {
  await BrowserTestUtils.withNewTab("about:preferences", async browser => {
    let sandbox = sinon.createSandbox();
    let disableEncryptionStub = sandbox
      .stub(BackupService.prototype, "disableEncryption")
      .resolves(true);
    let deleteLastBackupStub = sandbox
      .stub(BackupService.prototype, "deleteLastBackup")
      .resolves(true);

    /* Unlike other tests, we'll pretend that encryption is enabled by stubbing
     * out the actual BackupService state, instead of the state passed to backup-settings
     * since we're not testing UI here. */
    const testDefaultName = "test-default-path";
    sandbox.stub(BackupService.prototype, "state").get(() => {
      return {
        encryptionEnabled: true,
        defaultParent: {
          path: PathUtils.join(PathUtils.tempDir, testDefaultName),
          fileName: testDefaultName,
        },
      };
    });

    await SpecialPowers.pushPrefEnv({
      set: [[SCHEDULED_BACKUPS_ENABLED_PREF, true]],
    });

    await turnOffScheduledBackupsHelper(browser, () => {
      Assert.ok(
        disableEncryptionStub.calledOnce,
        "BackupService was called to disable encryption"
      );
      Assert.ok(
        deleteLastBackupStub.calledOnce,
        "BackupService was called to delete the latest backup file"
      );
    });

    await SpecialPowers.popPrefEnv();
    sandbox.restore();
  });
});
