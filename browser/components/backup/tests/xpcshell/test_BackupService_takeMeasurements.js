/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(() => {
  // FOG needs to be initialized in order for data to flow.
  Services.fog.initializeFOG();
  Services.telemetry.clearScalars();
});

/**
 * Tests that calling `BackupService.takeMeasurements` will call the measure
 * method of all registered BackupResource classes.
 */
add_task(async function test_takeMeasurements() {
  let sandbox = sinon.createSandbox();
  sandbox.stub(FakeBackupResource1.prototype, "measure").resolves();
  sandbox
    .stub(FakeBackupResource2.prototype, "measure")
    .rejects(new Error("Some failure to measure"));

  let bs = new BackupService({ FakeBackupResource1, FakeBackupResource2 });
  await bs.takeMeasurements();

  for (let backupResourceClass of [FakeBackupResource1, FakeBackupResource2]) {
    Assert.ok(
      backupResourceClass.prototype.measure.calledOnce,
      "Measure was called"
    );
    Assert.ok(
      backupResourceClass.prototype.measure.calledWith(PathUtils.profileDir),
      "Measure was called with the profile directory argument"
    );
  }

  sandbox.restore();
});

/**
 * Tests that we can measure the disk space available in the profile directory.
 */
add_task(async function test_profDDiskSpace() {
  Services.telemetry.clearScalars();

  let bs = new BackupService();
  await bs.takeMeasurements();
  let measurement = Glean.browserBackup.profDDiskSpace.testGetValue();
  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("parent", false, true),
    "browser.backup.prof_d_disk_space",
    measurement
  );

  Assert.greater(
    measurement,
    0,
    "Should have collected a measurement for the profile directory storage " +
      "device"
  );
});

/**
 * Tests that we record a scalar if the BackupService is configured to
 * initialize on launch.
 */
add_task(async function test_BackupService_enabled_state() {
  Services.telemetry.clearScalars();

  let bs = new BackupService();
  await bs.takeMeasurements();
  Assert.ok(
    Glean.browserBackup.enabled.testGetValue(),
    "Should have set the enabled scalar."
  );
  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("parent", false, true),
    "browser.backup.enabled",
    true,
    "Should have set the enabled scalar in legacy Telemetry."
  );
});

/**
 * Tests that we record a scalar if the BackupService is configured to
 * initialize on launch.
 */
add_task(async function test_BackupService_scheduler_enabled_state() {
  Services.telemetry.clearScalars();
  const SCHEDULED_BACKUPS_ENABLED_PREF_NAME =
    "browser.backup.scheduled.enabled";
  Services.prefs.setBoolPref(SCHEDULED_BACKUPS_ENABLED_PREF_NAME, false);

  let bs = new BackupService();
  await bs.takeMeasurements();
  Assert.ok(
    !Glean.browserBackup.schedulerEnabled.testGetValue(),
    "Scalar for scheduled backups should be false"
  );
  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("parent", false, true),
    "browser.backup.scheduler_enabled",
    false,
    "Scalar for scheduled backups should be false in legacy Telemetry."
  );

  Services.telemetry.clearScalars();
  Services.prefs.setBoolPref(SCHEDULED_BACKUPS_ENABLED_PREF_NAME, true);
  await bs.takeMeasurements();
  Assert.ok(
    Glean.browserBackup.schedulerEnabled.testGetValue(),
    "Scalar for scheduled backups should be true"
  );
  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("parent", false, true),
    "browser.backup.scheduler_enabled",
    true,
    "Scalar for scheduled backups should be true in legacy Telemetry."
  );

  // Now reset the scheduling state to the default to not interfere with
  // other tests.
  Services.prefs.clearUserPref(SCHEDULED_BACKUPS_ENABLED_PREF_NAME);
});

/**
 * Tests that we record a scalar if the BackupService is configured to
 * encrypt backups.
 */
add_task(async function test_BackupService_pswd_encrypted_state() {
  Services.telemetry.clearScalars();

  let bs = new BackupService();
  await bs.takeMeasurements();
  Assert.ok(
    !Glean.browserBackup.pswdEncrypted.testGetValue(),
    "Scalar for encrypted backups should be false"
  );
  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("parent", false, true),
    "browser.backup.pswd_encrypted",
    false,
    "Scalar for encrypted backups should be false in legacy Telemetry."
  );

  Services.telemetry.clearScalars();
  const tempDir = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "BackupService-takeMeasurements-test"
  );
  await bs.enableEncryption("some-fake-password", tempDir);

  await bs.takeMeasurements();
  Assert.ok(
    Glean.browserBackup.pswdEncrypted.testGetValue(),
    "Scalar for encrypted backups should be true"
  );
  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("parent", false, true),
    "browser.backup.pswd_encrypted",
    true,
    "Scalar for encrypted backups should be true in legacy Telemetry."
  );

  await maybeRemovePath(tempDir);
});

/**
 * Tests that we record a scalar that tells us if backups are configured to
 * be written to the default location, or somewhere else entirely.
 */
add_task(async function test_BackupService_location_on_device() {
  Services.telemetry.clearScalars();
  const DEFAULT_LOCATION = 1;
  const NON_DEFAULT_LOCATION = 2;

  let bs = new BackupService();
  bs.setParentDirPath(PathUtils.tempDir);

  await bs.takeMeasurements();
  Assert.equal(
    Glean.browserBackup.locationOnDevice.testGetValue(),
    NON_DEFAULT_LOCATION,
    "Scalar for location on device should indicate the non-default " +
      "location"
  );
  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("parent", false, true),
    "browser.backup.location_on_device",
    NON_DEFAULT_LOCATION,
    "Scalar for location on device should indicate the non-default " +
      "location in legacy Telemetry."
  );

  Services.telemetry.clearScalars();
  // The system "Docs" folder is considered the default location.
  bs.setParentDirPath(Services.dirsvc.get("Docs", Ci.nsIFile).path);

  await bs.takeMeasurements();
  Assert.equal(
    Glean.browserBackup.locationOnDevice.testGetValue(),
    DEFAULT_LOCATION,
    "Scalar for location on device should indicate the default location"
  );
  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("parent", false, true),
    "browser.backup.location_on_device",
    DEFAULT_LOCATION,
    "Scalar for location on device should indicate the default " +
      "location in legacy Telemetry."
  );
});
