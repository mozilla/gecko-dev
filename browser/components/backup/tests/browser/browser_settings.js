/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { MockRegistrar } = ChromeUtils.importESModule(
  "resource://testing-common/MockRegistrar.sys.mjs"
);

const SCHEDULED_BACKUPS_ENABLED_PREF = "browser.backup.scheduled.enabled";

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
  Services.telemetry.clearEvents();
  Services.fog.testResetFOG();

  await BrowserTestUtils.withNewTab("about:preferences", async browser => {
    let sandbox = sinon.createSandbox();
    let disableEncryptionStub = sandbox
      .stub(BackupService.prototype, "disableEncryption")
      .resolves(true);

    await SpecialPowers.pushPrefEnv({
      set: [[SCHEDULED_BACKUPS_ENABLED_PREF, true]],
    });

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
    let promise = BrowserTestUtils.waitForEvent(
      window,
      "BackupUI:DisableEncryption"
    );

    Assert.ok(confirmButton, "Confirm button should be found");

    confirmButton.click();

    await promise;
    await settings.updateComplete;

    Assert.ok(
      disableEncryptionStub.calledOnce,
      "BackupService was called to disable encryption"
    );

    let legacyEvents = TelemetryTestUtils.getEvents(
      {
        category: "browser.backup",
        method: "password_removed",
        object: "BackupService",
      },
      { process: "parent" }
    );
    Assert.equal(
      legacyEvents.length,
      1,
      "Found the password_removed legacy event."
    );
    let events = Glean.browserBackup.passwordRemoved.testGetValue();
    Assert.equal(events.length, 1, "Found the passwordRemoved Glean event.");

    sandbox.restore();
    await SpecialPowers.popPrefEnv();
  });
});

/**
 * Tests that the a backup file can be restored from the settings page.
 */
add_task(async function test_restore_from_backup() {
  await BrowserTestUtils.withNewTab("about:preferences", async browser => {
    let sandbox = sinon.createSandbox();
    let recoverFromBackupArchiveStub = sandbox
      .stub(BackupService.prototype, "recoverFromBackupArchive")
      .resolves();

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

    Assert.ok(
      recoverFromBackupArchiveStub.calledOnce,
      "BackupService was called to start a recovery from a backup archive."
    );

    sandbox.restore();
  });
});

/**
 * Tests that the most recent backup information is shown inside of the
 * component. Also tests that the "Show in folder" and "Edit" buttons open
 * file pickers.
 */
add_task(async function test_last_backup_info_and_location() {
  // We'll override the default location for writing backup archives so that
  // we don't pollute this machine's Documents folder.
  const TEST_PROFILE_PATH = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "testLastBackupInfo"
  );

  await SpecialPowers.pushPrefEnv({
    set: [[SCHEDULED_BACKUPS_ENABLED_PREF, true]],
  });

  await BrowserTestUtils.withNewTab("about:preferences", async browser => {
    let sandbox = sinon.createSandbox();
    let bs = BackupService.get();

    await SpecialPowers.pushPrefEnv({
      set: [["browser.backup.location", TEST_PROFILE_PATH]],
    });

    Assert.ok(bs.state.backupDirPath, "Backup Dir Path was set");

    let settings = browser.contentDocument.querySelector("backup-settings");
    await settings.updateComplete;

    let stateUpdated = BrowserTestUtils.waitForEvent(
      bs,
      "BackupService:StateUpdate",
      false,
      () => {
        return bs.state.lastBackupDate && bs.state.lastBackupFileName;
      }
    );
    let { archivePath } = await bs.createBackup();
    registerCleanupFunction(async () => {
      // No matter what happens after this, make sure to clean this file up.
      await IOUtils.remove(archivePath);
    });
    await stateUpdated;

    await settings.updateComplete;

    let dateArgs = JSON.parse(
      settings.lastBackupDateEl.getAttribute("data-l10n-args")
    );
    // The lastBackupDate is stored in seconds, but Fluent expects milliseconds,
    // so we'll check that it was converted.
    Assert.equal(
      dateArgs.date,
      bs.state.lastBackupDate * 1000,
      "Should have the backup date as a Fluent arg, in milliseconds"
    );

    let locationArgs = JSON.parse(
      settings.lastBackupFileNameEl.getAttribute("data-l10n-args")
    );
    Assert.equal(
      locationArgs.fileName,
      bs.state.lastBackupFileName,
      "Should have the backup file name as a Fluent arg"
    );

    // Mocking out nsLocalFile isn't something that works very well, so we'll
    // just stub out BackupService.showBackupLocation with something that'll
    // resolve showBackupLocationPromise, and rely on manual testing for
    // showing the location of the backup file.
    let showBackupLocationPromise = new Promise(resolve => {
      let showBackupLocationStub = sandbox.stub(bs, "showBackupLocation");
      showBackupLocationStub.callsFake(() => {
        resolve();
      });
    });

    settings.backupLocationShowButtonEl.click();
    await showBackupLocationPromise;

    const TEST_NEW_BACKUP_PARENT_PATH = await IOUtils.createUniqueDirectory(
      PathUtils.tempDir,
      "testNewBackupParent"
    );
    let newBackupParent = await IOUtils.getDirectory(
      TEST_NEW_BACKUP_PARENT_PATH
    );

    stateUpdated = BrowserTestUtils.waitForEvent(
      bs,
      "BackupService:StateUpdate",
      false,
      () => {
        return bs.state.backupDirPath.startsWith(TEST_NEW_BACKUP_PARENT_PATH);
      }
    );
    let filePickerShownPromise = new Promise(resolve => {
      MockFilePicker.showCallback = async () => {
        Assert.ok(true, "Filepicker shown");
        MockFilePicker.setFiles([newBackupParent]);
        resolve();
      };
    });
    MockFilePicker.returnValue = MockFilePicker.returnOK;

    settings.backupLocationEditButtonEl.click();
    await filePickerShownPromise;
    await stateUpdated;

    await IOUtils.remove(TEST_NEW_BACKUP_PARENT_PATH);
    sandbox.restore();
  });
});
