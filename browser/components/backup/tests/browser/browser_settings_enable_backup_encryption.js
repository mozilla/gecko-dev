/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the enable-backup-encryption dialog can enable encryption
 * from the settings page via the toggle checkbox.
 */
add_task(async function test_enable_backup_encryption_checkbox_confirm() {
  await BrowserTestUtils.withNewTab("about:preferences", async browser => {
    let sandbox = sinon.createSandbox();
    let enableEncryptionStub = sandbox
      .stub(BackupService.prototype, "enableEncryption")
      .resolves(true);

    let settings = browser.contentDocument.querySelector("backup-settings");

    /**
     * For this test, we can pretend that browser-settings receives a backupServiceState
     * with encryptionEnable set to false. Normally, Lit only detects reactive property updates if a
     * property's reference changes (ex. completely replace backupServiceState with a new object),
     * which we actually do after calling BackupService.stateUpdate() and BackupUIParent.sendState().
     *
     * Since we only care about encryptionEnabled, we can just call Lit's requestUpdate() to force
     * the update explicitly.
     */
    settings.backupServiceState.encryptionEnabled = false;

    await settings.requestUpdate();
    await settings.updateComplete;

    let sensitiveDataCheckbox = settings.sensitiveDataCheckboxInputEl;
    Assert.ok(sensitiveDataCheckbox, "Sensitive data checkbox should be found");
    Assert.ok(
      !sensitiveDataCheckbox.checked,
      "Sensitive data checkbox should not be checked"
    );

    sensitiveDataCheckbox.click();
    await settings.updateComplete;

    let enableBackupEncryptionDialog = settings.enableBackupEncryptionDialogEl;
    Assert.ok(
      enableBackupEncryptionDialog?.open,
      "enable-backup-encryption-dialog should be open"
    );

    let enableBackupEncryption = settings.enableBackupEncryptionEl;
    Assert.ok(
      enableBackupEncryption,
      "enable-backup-encryption should be found"
    );

    let newPasswordInput = enableBackupEncryption.inputNewPasswordEl;
    let repeatPasswordInput = enableBackupEncryption.inputRepeatPasswordEl;

    // Pretend we're entering a password in the new password field
    let newPassPromise = createMockPassInputEventPromise(
      newPasswordInput,
      MOCK_PASSWORD
    );
    await newPassPromise;

    // Pretend we're entering a password in the repeat field
    // Before matching passwords, verify confirm button
    let confirmButton = enableBackupEncryption.confirmButtonEl;
    Assert.ok(confirmButton, "Confirm button should be found");
    Assert.ok(confirmButton.disabled, "Confirm button should be disabled");

    // Passwords match
    let matchPassPromise = createMockPassInputEventPromise(
      repeatPasswordInput,
      MOCK_PASSWORD
    );
    await matchPassPromise;

    let confirmButtonPromise = BrowserTestUtils.waitForMutationCondition(
      confirmButton,
      { attributes: true },
      () => !confirmButton.disabled
    );

    await confirmButtonPromise;
    ok(!confirmButton.disabled, "Confirm button should no longer be disabled");

    await settings.updateComplete;
    confirmButton = settings.enableBackupEncryptionEl.confirmButtonEl;

    let encryptionPromise = BrowserTestUtils.waitForEvent(
      window,
      "enableEncryption"
    );

    confirmButton.click();

    await encryptionPromise;

    Assert.ok(
      enableEncryptionStub.calledOnceWith(MOCK_PASSWORD),
      "BackupService was called to enable encryption with inputted password"
    );
    sandbox.restore();
  });
});

/**
 * Tests that the enable-backup-encryption dialog can enable encryption
 * from the settings page via the change password button.
 */
add_task(
  async function test_enable_backup_encryption_change_password_confirm() {
    await BrowserTestUtils.withNewTab("about:preferences", async browser => {
      let sandbox = sinon.createSandbox();
      let enableEncryptionStub = sandbox
        .stub(BackupService.prototype, "enableEncryption")
        .resolves(true);
      let disableEncryptionStub = sandbox
        .stub(BackupService.prototype, "disableEncryption")
        .resolves(true);

      let settings = browser.contentDocument.querySelector("backup-settings");
      settings.backupServiceState.encryptionEnabled = true;

      await settings.requestUpdate();
      await settings.updateComplete;

      let changePasswordButton = settings.changePasswordButtonEl;
      Assert.ok(changePasswordButton, "Change password button should be found");

      changePasswordButton.click();
      await settings.updateComplete;

      let enableBackupEncryptionDialog =
        settings.enableBackupEncryptionDialogEl;
      Assert.ok(
        enableBackupEncryptionDialog?.open,
        "enable-backup-encryption-dialog should be open"
      );

      let enableBackupEncryption = settings.enableBackupEncryptionEl;
      Assert.ok(
        enableBackupEncryption,
        "enable-backup-encryption should be found"
      );

      let newPasswordInput = enableBackupEncryption.inputNewPasswordEl;
      let repeatPasswordInput = enableBackupEncryption.inputRepeatPasswordEl;
      const changedPassword = "changedPassword";

      // Pretend we're entering a password in the new password field
      let newPassPromise = createMockPassInputEventPromise(
        newPasswordInput,
        changedPassword
      );
      await newPassPromise;

      // Pretend we're entering a password in the repeat field
      // Before matching passwords, verify confirm button
      let confirmButton = enableBackupEncryption.confirmButtonEl;
      Assert.ok(confirmButton, "Confirm button should be found");
      Assert.ok(confirmButton.disabled, "Confirm button should be disabled");

      // Passwords match
      let matchPassPromise = createMockPassInputEventPromise(
        repeatPasswordInput,
        changedPassword
      );
      await matchPassPromise;

      let confirmButtonPromise = BrowserTestUtils.waitForMutationCondition(
        confirmButton,
        { attributes: true },
        () => !confirmButton.disabled
      );

      await confirmButtonPromise;
      ok(
        !confirmButton.disabled,
        "Confirm button should no longer be disabled"
      );

      await settings.updateComplete;
      confirmButton = settings.enableBackupEncryptionEl.confirmButtonEl;

      let promise = BrowserTestUtils.waitForEvent(window, "rerunEncryption");
      confirmButton.click();
      await promise;

      Assert.ok(
        disableEncryptionStub.calledOnce,
        "BackupService was called to disable encryption first before registering the changed password"
      );
      Assert.ok(
        enableEncryptionStub.calledOnceWith(changedPassword),
        "BackupService was called to re-run encryption with changed password"
      );
      sandbox.restore();
    });
  }
);
