/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async () => {
  MockFilePicker.init(window.browsingContext);
  registerCleanupFunction(() => {
    MockFilePicker.cleanup();
  });
});

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
 * Tests that the disable-backup-encryption dialog can disable encryption
 * from the settings page.
 */
add_task(async function test_disable_backup_encryption_confirm() {
  await BrowserTestUtils.withNewTab("about:preferences", async browser => {
    let sandbox = sinon.createSandbox();
    let disableEncryptionStub = sandbox
      .stub(BackupService.prototype, "disableEncryption")
      .resolves(true);

    let settings = browser.contentDocument.querySelector("backup-settings");

    /**
     * For this test, we can pretend that browser-settings receives a backupServiceState
     * with encryptionEnable set to true. Normally, Lit only detects reactive property updates if a
     * property's reference changes (ex. completely replace backupServiceState with a new object),
     * which we actually do after calling BackupService.stateUpdate() and BackupUIParent.sendState().
     *
     * Since we only care about encryptionEnabled, we can just call Lit's requestUpdate() to force
     * the update explicitly.
     */
    settings.backupServiceState.encryptionEnabled = true;

    await settings.requestUpdate();
    await settings.updateComplete;

    let sensitiveDataCheckbox = settings.sensitiveDataCheckboxInputEl;

    Assert.ok(sensitiveDataCheckbox, "Sensitive data checkbox should be found");

    Assert.ok(
      sensitiveDataCheckbox.checked,
      "Sensitive data checkbox should be checked"
    );

    let disableBackupEncryption = settings.disableBackupEncryptionEl;

    Assert.ok(
      disableBackupEncryption,
      "disable-backup-encryption should be found"
    );

    let confirmButton = disableBackupEncryption.confirmButtonEl;
    let promise = BrowserTestUtils.waitForEvent(window, "disableEncryption");

    Assert.ok(confirmButton, "Confirm button should be found");

    confirmButton.click();

    await promise;
    await settings.updateComplete;

    Assert.ok(
      disableEncryptionStub.calledOnce,
      "BackupService was called to disable encryption"
    );
    sandbox.restore();
  });
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

/**
 * Tests that the a backup file can be restored from the settings page.
 */
add_task(async function test_restore_from_backup() {
  await BrowserTestUtils.withNewTab("about:preferences", async browser => {
    const mockBackupFilePath = await IOUtils.createUniqueFile(
      PathUtils.tempDir,
      "backup.html"
    );
    const mockBackupFile = Cc["@mozilla.org/file/local;1"].createInstance(
      Ci.nsIFile
    );
    mockBackupFile.initWithPath(mockBackupFilePath);

    let filePickerShownPromise = new Promise(resolve => {
      MockFilePicker.showCallback = async () => {
        Assert.ok(true, "Filepicker shown");
        MockFilePicker.setFiles([mockBackupFile]);
        resolve();
      };
    });
    MockFilePicker.returnValue = MockFilePicker.returnOK;

    let settings = browser.contentDocument.querySelector("backup-settings");

    await settings.updateComplete;

    Assert.ok(
      settings.restoreFromBackupButtonEl,
      "Button to restore backups should be found"
    );

    settings.restoreFromBackupButtonEl.click();

    await settings.updateComplete;

    let restoreFromBackup = settings.restoreFromBackupEl;

    Assert.ok(restoreFromBackup, "restore-from-backup should be found");

    let infoPromise = BrowserTestUtils.waitForEvent(
      window,
      "getBackupFileInfo"
    );

    restoreFromBackup.chooseButtonEl.click();
    await filePickerShownPromise;

    await infoPromise;
    // Set mock file info
    restoreFromBackup.backupFileInfo = {
      date: new Date(),
      isEncrypted: true,
    };
    await restoreFromBackup.updateComplete;

    // Set password for file
    restoreFromBackup.passwordInput.value = "h-*@Vfge3_hGxdpwqr@w";

    let restorePromise = BrowserTestUtils.waitForEvent(
      window,
      "restoreFromBackupConfirm"
    );

    Assert.ok(
      restoreFromBackup.confirmButtonEl,
      "Confirm button should be found"
    );

    await restoreFromBackup.updateComplete;
    restoreFromBackup.confirmButtonEl.click();

    await restorePromise.then(e => {
      let mockEvent = {
        backupFile: mockBackupFile.path,
        backupPassword: "h-*@Vfge3_hGxdpwqr@w",
      };
      Assert.deepEqual(
        e.detail,
        mockEvent,
        "Event should contain the file and password"
      );
    });
  });
});
