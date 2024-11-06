/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ERRORS } = ChromeUtils.importESModule(
  "chrome://browser/content/backup/backup-constants.mjs"
);

const SCHEDULED_BACKUPS_ENABLED_PREF = "browser.backup.scheduled.enabled";

add_setup(async () => {
  MockFilePicker.init(window.browsingContext);
  registerCleanupFunction(() => {
    MockFilePicker.cleanup();
  });
});

/**
 * Tests that the turn on scheduled backups dialog can set
 * browser.backup.scheduled.enabled to true from the settings page.
 */
add_task(async function test_turn_on_scheduled_backups_confirm() {
  Services.telemetry.clearEvents();
  Services.fog.testResetFOG();

  await BrowserTestUtils.withNewTab("about:preferences", async browser => {
    let settings = browser.contentDocument.querySelector("backup-settings");

    await settings.updateComplete;

    let turnOnButton = settings.scheduledBackupsButtonEl;

    Assert.ok(
      turnOnButton,
      "Button to turn on scheduled backups should be found"
    );

    turnOnButton.click();

    await settings.updateComplete;

    let turnOnScheduledBackups = settings.turnOnScheduledBackupsEl;

    Assert.ok(
      turnOnScheduledBackups,
      "turn-on-scheduled-backups should be found"
    );

    let confirmButton = turnOnScheduledBackups.confirmButtonEl;
    let promise = BrowserTestUtils.waitForEvent(
      window,
      "BackupUI:EnableScheduledBackups"
    );

    Assert.ok(confirmButton, "Confirm button should be found");

    confirmButton.click();

    await promise;
    await settings.updateComplete;

    let scheduledPrefVal = Services.prefs.getBoolPref(
      SCHEDULED_BACKUPS_ENABLED_PREF
    );
    Assert.ok(scheduledPrefVal, "Scheduled backups pref should be true");

    let legacyEvents = TelemetryTestUtils.getEvents(
      {
        category: "browser.backup",
        method: "toggle_on",
        object: "BackupService",
      },
      { process: "parent" }
    );
    Assert.equal(legacyEvents.length, 1, "Found the toggle_on legacy event.");
    let events = Glean.browserBackup.toggleOn.testGetValue();
    Assert.equal(events.length, 1, "Found the toggleOn Glean event.");

    // Reset scheduled backups again for subsequent tests.
    Services.prefs.clearUserPref(SCHEDULED_BACKUPS_ENABLED_PREF);
  });
});

/**
 * Tests that the turn on scheduled backups dialog displays the default input field
 * and a filepicker to choose a custom backup file path, updates the input field to show
 * that path, and sets browser.backup.location to the path from the settings page.
 */
add_task(async function test_turn_on_custom_location_filepicker() {
  Services.telemetry.clearEvents();
  Services.fog.testResetFOG();

  await BrowserTestUtils.withNewTab("about:preferences", async browser => {
    const mockCustomParentDir = await IOUtils.createUniqueDirectory(
      PathUtils.tempDir,
      "settings-custom-dir-test"
    );
    const dummyFile = Cc["@mozilla.org/file/local;1"].createInstance(
      Ci.nsIFile
    );

    dummyFile.initWithPath(mockCustomParentDir);
    let filePickerShownPromise = new Promise(resolve => {
      MockFilePicker.showCallback = () => {
        Assert.ok(true, "Filepicker shown");
        MockFilePicker.setFiles([dummyFile]);
        resolve();
      };
    });
    MockFilePicker.returnValue = MockFilePicker.returnOK;

    // After setting up mocks, start testing components
    /** @type {import("../../content/backup-settings.mjs").default} */
    let settings = browser.contentDocument.querySelector("backup-settings");
    let turnOnButton = settings.scheduledBackupsButtonEl;

    Assert.ok(
      turnOnButton,
      "Button to turn on scheduled backups should be found"
    );

    await settings.updateComplete;
    let turnOnScheduledBackups = settings.turnOnScheduledBackupsEl;

    Assert.ok(
      turnOnScheduledBackups,
      "turn-on-scheduled-backups should be found"
    );

    // First verify the default input value and dir path button
    let filePathInputDefault = turnOnScheduledBackups.filePathInputDefaultEl;
    let filePathButton = turnOnScheduledBackups.filePathButtonEl;
    const documentsPath = BackupService.DEFAULT_PARENT_DIR_PATH;

    Assert.ok(
      filePathInputDefault,
      "Default input for choosing a file path should be found"
    );
    Assert.equal(
      filePathInputDefault.value,
      `${PathUtils.filename(documentsPath)} (recommended)`,
      "Default input displays the expected text"
    );
    Assert.ok(
      filePathButton,
      "Button for choosing a file path should be found"
    );

    // Next, verify the filepicker and updated dialog
    let inputUpdatePromise = BrowserTestUtils.waitForCondition(
      () => turnOnScheduledBackups.filePathInputCustomEl
    );

    filePathButton.click();

    await filePickerShownPromise;
    await turnOnScheduledBackups.updateComplete;

    info("Waiting for file path input to update");
    await inputUpdatePromise;
    Assert.ok("Input should have been updated");

    let filePathInputCustom = turnOnScheduledBackups.filePathInputCustomEl;
    Assert.equal(
      filePathInputCustom.value,
      PathUtils.filename(mockCustomParentDir),
      "Input should display file path from filepicker"
    );

    // Now close the dialog by confirming choices and verify that backup settings are saved
    let confirmButton = turnOnScheduledBackups.confirmButtonEl;
    Assert.ok(confirmButton, "Confirm button should be found");

    let confirmButtonPromise = BrowserTestUtils.waitForEvent(
      window,
      "BackupUI:EnableScheduledBackups"
    );

    confirmButton.click();

    await confirmButtonPromise;
    await settings.updateComplete;

    // Backup folder should be joined with the updated path
    let locationPrefVal = Services.prefs.getStringPref(
      "browser.backup.location"
    );
    Assert.equal(
      locationPrefVal,
      PathUtils.join(mockCustomParentDir, BackupService.BACKUP_DIR_NAME),
      "Backup location pref should be updated"
    );

    await IOUtils.remove(mockCustomParentDir, {
      ignoreAbsent: true,
      recursive: true,
    });

    let legacyEvents = TelemetryTestUtils.getEvents(
      {
        category: "browser.backup",
        method: "toggle_on",
        object: "BackupService",
      },
      { process: "parent" }
    );
    Assert.equal(legacyEvents.length, 1, "Found the toggle_on legacy event.");
    let events = Glean.browserBackup.toggleOn.testGetValue();
    Assert.equal(events.length, 1, "Found the toggleOn Glean event.");

    legacyEvents = TelemetryTestUtils.getEvents(
      {
        category: "browser.backup",
        method: "change_location",
        object: "BackupService",
      },
      { process: "parent" }
    );
    Assert.equal(
      legacyEvents.length,
      1,
      "Found the change_location legacy event."
    );
    events = Glean.browserBackup.changeLocation.testGetValue();
    Assert.equal(events.length, 1, "Found the changeLocation Glean event.");

    // Reset scheduled backups again for subsequent tests.
    Services.prefs.clearUserPref(SCHEDULED_BACKUPS_ENABLED_PREF);
  });
});

/**
 * Tests that encryption is enabled after entering a password in the
 * turn-on-scheduled-backups dialog.
 */
add_task(async function test_turn_on_scheduled_backups_encryption() {
  Services.telemetry.clearEvents();
  Services.fog.testResetFOG();

  await BrowserTestUtils.withNewTab("about:preferences", async browser => {
    let sandbox = sinon.createSandbox();
    let settings = browser.contentDocument.querySelector("backup-settings");

    await settings.updateComplete;

    let turnOnButton = settings.scheduledBackupsButtonEl;
    Assert.ok(
      turnOnButton,
      "Button to turn on scheduled backups should be found"
    );

    turnOnButton.click();
    await settings.updateComplete;

    let turnOnScheduledBackups = settings.turnOnScheduledBackupsEl;
    Assert.ok(
      turnOnScheduledBackups,
      "turn-on-scheduled-backups should be found"
    );

    let encryptionStub = sandbox
      .stub(BackupService.prototype, "enableEncryption")
      .resolves(true);

    // Enable passwords
    let passwordsCheckbox = turnOnScheduledBackups.passwordOptionsCheckboxEl;
    passwordsCheckbox.click();
    await turnOnScheduledBackups.updateComplete;

    let passwordOptionsExpanded =
      turnOnScheduledBackups.passwordOptionsExpandedEl;
    Assert.ok(passwordOptionsExpanded, "Password inputs should be found");

    let validityPromise = createMockValidityPassEventPromise(
      turnOnScheduledBackups,
      passwordOptionsExpanded,
      "ValidPasswordsDetected"
    );

    // Verify confirm button
    let confirmButton = turnOnScheduledBackups.confirmButtonEl;
    Assert.ok(confirmButton, "Confirm button should be found");
    Assert.ok(confirmButton.disabled, "Confirm button should be disabled");

    let confirmButtonPromise = BrowserTestUtils.waitForMutationCondition(
      confirmButton,
      { attributes: true },
      () => !confirmButton.disabled
    );

    await validityPromise;
    await confirmButtonPromise;

    let promise = BrowserTestUtils.waitForEvent(
      window,
      "BackupUI:EnableScheduledBackups"
    );

    confirmButton.click();

    await promise;
    await settings.updateComplete;

    Assert.ok(
      encryptionStub.calledOnceWith(MOCK_PASSWORD),
      "BackupService was called to enable encryption and received the expected argument"
    );

    let legacyEvents = TelemetryTestUtils.getEvents(
      {
        category: "browser.backup",
        method: "toggle_on",
        object: "BackupService",
      },
      { process: "parent" }
    );
    Assert.equal(legacyEvents.length, 1, "Found the toggle_on legacy event.");
    let events = Glean.browserBackup.toggleOn.testGetValue();
    Assert.equal(events.length, 1, "Found the toggleOn Glean event.");

    legacyEvents = TelemetryTestUtils.getEvents(
      {
        category: "browser.backup",
        method: "password_added",
        object: "BackupService",
      },
      { process: "parent" }
    );
    Assert.equal(
      legacyEvents.length,
      1,
      "Found the password_added legacy event."
    );
    events = Glean.browserBackup.passwordAdded.testGetValue();
    Assert.equal(events.length, 1, "Found the passwordAdded Glean event.");

    sandbox.restore();
    Services.prefs.clearUserPref(SCHEDULED_BACKUPS_ENABLED_PREF);
  });
});

/**
 * Tests that scheduled backups are not enabled if there is an issue with
 * enabling encryption.
 */
add_task(async function test_turn_on_scheduled_backups_encryption_error() {
  await BrowserTestUtils.withNewTab("about:preferences", async browser => {
    let sandbox = sinon.createSandbox();
    let settings = browser.contentDocument.querySelector("backup-settings");

    await settings.updateComplete;

    let turnOnButton = settings.scheduledBackupsButtonEl;
    Assert.ok(
      turnOnButton,
      "Button to turn on scheduled backups should be found"
    );

    turnOnButton.click();
    await settings.updateComplete;

    let turnOnScheduledBackups = settings.turnOnScheduledBackupsEl;
    Assert.ok(
      turnOnScheduledBackups,
      "turn-on-scheduled-backups should be found"
    );

    let encryptionStub = sandbox
      .stub(BackupService.prototype, "enableEncryption")
      .throws(new Error("test error", { cause: ERRORS.INVALID_PASSWORD }));

    // Enable passwords
    let passwordsCheckbox = turnOnScheduledBackups.passwordOptionsCheckboxEl;
    passwordsCheckbox.click();
    await turnOnScheduledBackups.updateComplete;

    let passwordOptionsExpanded =
      turnOnScheduledBackups.passwordOptionsExpandedEl;

    Assert.ok(
      passwordOptionsExpanded,
      "Passwords expanded options should be found"
    );

    let validityPromise = createMockValidityPassEventPromise(
      turnOnScheduledBackups,
      passwordOptionsExpanded,
      "ValidPasswordsDetected"
    );

    // Verify confirm button
    let confirmButton = turnOnScheduledBackups.confirmButtonEl;
    Assert.ok(confirmButton, "Confirm button should be found");
    Assert.ok(confirmButton.disabled, "Confirm button should be disabled");

    let confirmButtonPromise = BrowserTestUtils.waitForMutationCondition(
      confirmButton,
      { attributes: true },
      () => !confirmButton.disabled
    );

    await validityPromise;
    await confirmButtonPromise;

    let promise = BrowserTestUtils.waitForEvent(
      window,
      "BackupUI:EnableScheduledBackups"
    );

    confirmButton.click();

    await promise;
    await settings.updateComplete;

    Assert.ok(
      encryptionStub.threw(),
      "BackupService threw an error during encryption"
    );

    // Ensure that the scheduled backups pref is not updated.
    let scheduledPrefVal = Services.prefs.getBoolPref(
      SCHEDULED_BACKUPS_ENABLED_PREF
    );
    Assert.ok(
      !scheduledPrefVal,
      "Scheduled backups pref should still be false"
    );

    await BrowserTestUtils.waitForCondition(
      () => !!turnOnScheduledBackups.errorEl,
      "Error should be displayed to the user"
    );

    Assert.ok(
      turnOnScheduledBackups.errorEl,
      "Error should be displayed to the user"
    );

    sandbox.restore();
    Services.prefs.clearUserPref(SCHEDULED_BACKUPS_ENABLED_PREF);
  });
});
