/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { BackupService } = ChromeUtils.importESModule(
  "resource:///modules/backup/BackupService.sys.mjs"
);

const { MockFilePicker } = SpecialPowers;

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
 * Tests that the turn on scheduled backups dialog can set
 * browser.backup.scheduled.enabled to true from the settings page.
 */
add_task(async function test_turn_on_scheduled_backups_confirm() {
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
      "turnOnScheduledBackups"
    );

    Assert.ok(confirmButton, "Confirm button should be found");

    confirmButton.click();

    await promise;
    await settings.updateComplete;

    let scheduledPrefVal = Services.prefs.getBoolPref(
      "browser.backup.scheduled.enabled"
    );
    Assert.ok(scheduledPrefVal, "Scheduled backups pref should be true");
  });
});

/**
 * Tests that the turn on scheduled backups dialog displays the default input field
 * and a filepicker to choose a custom backup file path, updates the input field to show
 * that path, and sets browser.backup.location to the path from the settings page.
 */
add_task(async function test_turn_on_custom_location_filepicker() {
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
      "turnOnScheduledBackups"
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
  });
});

/**
 * Tests that the turn off scheduled backups dialog can set
 * browser.backup.scheduled.enabled to false from the settings page.
 */
add_task(async function test_turn_off_scheduled_backups_confirm() {
  await BrowserTestUtils.withNewTab("about:preferences", async browser => {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.backup.scheduled.enabled", true]],
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
      "browser.backup.scheduled.enabled"
    );
    Assert.ok(!scheduledPrefVal, "Scheduled backups pref should be false");
  });
});
