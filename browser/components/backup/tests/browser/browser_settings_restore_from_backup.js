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
 * Tests that the dialog stays open while restoring from the settings page.
 */
add_task(async function test_restore_in_progress() {
  await BrowserTestUtils.withNewTab("about:preferences", async browser => {
    let sandbox = sinon.createSandbox();
    let bs = BackupService.get();

    let { promise: recoverPromise, resolve: recoverResolve } =
      Promise.withResolvers();
    let recoverFromBackupArchiveStub = sandbox
      .stub(bs, "recoverFromBackupArchive")
      .returns(recoverPromise);

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
    Assert.ok(
      restoreFromBackup.confirmButtonEl.disabled,
      "Confirm button should be disabled."
    );

    const mockBackupFilePath = await IOUtils.createUniqueFile(
      PathUtils.tempDir,
      "backup.html"
    );

    // Set mock file
    restoreFromBackup.backupFileToRestore = mockBackupFilePath;
    await restoreFromBackup.updateComplete;

    Assert.ok(
      !restoreFromBackup.confirmButtonEl.disabled,
      "Confirm button should not be disabled."
    );
    Assert.equal(
      restoreFromBackup.confirmButtonEl.getAttribute("data-l10n-id"),
      "restore-from-backup-confirm-button",
      "Confirm button should show confirm message."
    );

    let restorePromise = BrowserTestUtils.waitForEvent(
      window,
      "restoreFromBackupConfirm"
    );

    restoreFromBackup.confirmButtonEl.click();
    let currentState = bs.state;
    let recoveryInProgressState = Object.assign(
      { recoveryInProgress: true },
      currentState
    );
    sandbox.stub(BackupService.prototype, "state").get(() => {
      return recoveryInProgressState;
    });
    bs.stateUpdate();

    await restorePromise;

    await settings.updateComplete;

    Assert.ok(
      settings.restoreFromBackupDialogEl.open,
      "Restore dialog should still be open."
    );

    Assert.ok(
      restoreFromBackup.confirmButtonEl.disabled,
      "Confirm button should be disabled."
    );

    Assert.equal(
      restoreFromBackup.confirmButtonEl.getAttribute("data-l10n-id"),
      "restore-from-backup-restoring-button",
      "Confirm button should show restoring message."
    );

    Assert.ok(
      recoverFromBackupArchiveStub.calledOnce,
      "BackupService was called to start a recovery from a backup archive."
    );

    // Now cause recovery to resolve.
    recoverResolve();
    // Wait a tick of the event loop to let the BackupUIParent respond to
    // the promise resolution, and to send its message to the BackupUIChild.
    await new Promise(resolve => SimpleTest.executeSoon(resolve));
    // Wait a second tick to let the BackupUIChild respond to the message
    // from BackupUIParent.
    await new Promise(resolve => SimpleTest.executeSoon(resolve));

    await settings.updateComplete;

    Assert.ok(
      !settings.restoreFromBackupDialogEl.open,
      "Restore dialog should now be closed."
    );

    sandbox.restore();
  });
});
