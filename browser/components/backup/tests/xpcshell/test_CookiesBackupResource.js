/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { CookiesBackupResource } = ChromeUtils.importESModule(
  "resource:///modules/backup/CookiesBackupResource.sys.mjs"
);

/**
 * Tests that we can measure the Cookies db in a profile directory.
 */
add_task(async function test_measure() {
  const EXPECTED_COOKIES_DB_SIZE = 1230;

  Services.fog.testResetFOG();

  // Create resource files in temporary directory
  let tempDir = PathUtils.tempDir;
  let tempCookiesDBPath = PathUtils.join(tempDir, "cookies.sqlite");
  await createKilobyteSizedFile(tempCookiesDBPath, EXPECTED_COOKIES_DB_SIZE);

  let cookiesBackupResource = new CookiesBackupResource();
  await cookiesBackupResource.measure(tempDir);

  let cookiesMeasurement = Glean.browserBackup.cookiesSize.testGetValue();
  let scalars = TelemetryTestUtils.getProcessScalars("parent", false, false);

  // Compare glean vs telemetry measurements
  TelemetryTestUtils.assertScalar(
    scalars,
    "browser.backup.cookies_size",
    cookiesMeasurement,
    "Glean and telemetry measurements for cookies.sqlite should be equal"
  );

  // Compare glean measurements vs actual file sizes
  Assert.equal(
    cookiesMeasurement,
    EXPECTED_COOKIES_DB_SIZE,
    "Should have collected the correct glean measurement for cookies.sqlite"
  );

  await maybeRemovePath(tempCookiesDBPath);
});

/**
 * Test that the backup method correctly copies items from the profile directory
 * into the staging directory.
 */
add_task(async function test_backup() {
  let sandbox = sinon.createSandbox();

  let cookiesBackupResource = new CookiesBackupResource();
  let sourcePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "CookiesBackupResource-source-test"
  );
  let stagingPath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "CookiesBackupResource-staging-test"
  );

  // Make sure this file exists in the source directory, otherwise
  // BackupResource will skip attempting to back it up.
  await createTestFiles(sourcePath, [{ path: "cookies.sqlite" }]);

  // We have no need to test that Sqlite.sys.mjs's backup method is working -
  // this is something that is tested in Sqlite's own tests. We can just make
  // sure that it's being called using sinon. Unfortunately, we cannot do the
  // same thing with IOUtils.copy, as its methods are not stubbable.
  let fakeConnection = {
    backup: sandbox.stub().resolves(true),
    close: sandbox.stub().resolves(true),
  };
  sandbox.stub(Sqlite, "openConnection").returns(fakeConnection);

  let manifestEntry = await cookiesBackupResource.backup(
    stagingPath,
    sourcePath
  );
  Assert.equal(
    manifestEntry,
    null,
    "CookiesBackupResource.backup should return null as its ManifestEntry"
  );

  // Next, we'll make sure that the Sqlite connection had `backup` called on it
  // with the right arguments.
  Assert.ok(
    fakeConnection.backup.calledOnce,
    "Called backup the expected number of times for all connections"
  );
  Assert.ok(
    fakeConnection.backup.calledWith(
      PathUtils.join(stagingPath, "cookies.sqlite")
    ),
    "Called backup on the cookies.sqlite Sqlite connection"
  );

  await maybeRemovePath(stagingPath);
  await maybeRemovePath(sourcePath);

  sandbox.restore();
});

/**
 * Tests that the backup method does not copy the cookie database if the
 * browser is configured to not save history - either while running, or to
 * clear it at shutdown.
 */
add_task(async function test_backup_no_saved_history() {
  let cookiesBackupResource = new CookiesBackupResource();
  let sourcePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "CookiesBackupResource-source-test"
  );
  let stagingPath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "CookiesBackupResource-staging-test"
  );

  let sandbox = sinon.createSandbox();
  let fakeConnection = {
    backup: sandbox.stub().resolves(true),
    close: sandbox.stub().resolves(true),
  };
  sandbox.stub(Sqlite, "openConnection").returns(fakeConnection);

  // First, we'll try with browsing history in general being disabled.
  Services.prefs.setBoolPref(HISTORY_ENABLED_PREF, false);
  Services.prefs.setBoolPref(SANITIZE_ON_SHUTDOWN_PREF, false);

  let manifestEntry = await cookiesBackupResource.backup(
    stagingPath,
    sourcePath
  );
  Assert.deepEqual(
    manifestEntry,
    null,
    "Should have gotten back a null ManifestEntry"
  );

  Assert.ok(
    fakeConnection.backup.notCalled,
    "No sqlite connections should have been made with remember history disabled"
  );

  // Now verify that the sanitize shutdown pref also prevents us from backing
  // up cookies
  Services.prefs.setBoolPref(HISTORY_ENABLED_PREF, true);
  Services.prefs.setBoolPref(SANITIZE_ON_SHUTDOWN_PREF, true);

  fakeConnection.backup.resetHistory();
  manifestEntry = await cookiesBackupResource.backup(stagingPath, sourcePath);
  Assert.deepEqual(
    manifestEntry,
    null,
    "Should have gotten back a null ManifestEntry"
  );

  Assert.ok(
    fakeConnection.backup.notCalled,
    "No sqlite connections should have been made with sanitize shutdown enabled"
  );

  await maybeRemovePath(stagingPath);
  await maybeRemovePath(sourcePath);

  sandbox.restore();
  Services.prefs.clearUserPref(HISTORY_ENABLED_PREF);
  Services.prefs.clearUserPref(SANITIZE_ON_SHUTDOWN_PREF);
});

/**
 * Tests that the backup method correctly skips backing up cookies when
 * permanent private browsing mode is enabled.
 */
add_task(async function test_backup_private_browsing() {
  let sandbox = sinon.createSandbox();

  let cookiesBackupResource = new CookiesBackupResource();
  let sourcePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "CookiesBackupResource-source-test"
  );
  let stagingPath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "CookiesBackupResource-staging-test"
  );

  let fakeConnection = {
    backup: sandbox.stub().resolves(true),
    close: sandbox.stub().resolves(true),
  };
  sandbox.stub(Sqlite, "openConnection").returns(fakeConnection);
  sandbox.stub(PrivateBrowsingUtils, "permanentPrivateBrowsing").value(true);

  let manifestEntry = await cookiesBackupResource.backup(
    stagingPath,
    sourcePath
  );
  Assert.deepEqual(
    manifestEntry,
    null,
    "Should have gotten back a null ManifestEntry"
  );

  Assert.ok(
    fakeConnection.backup.notCalled,
    "No sqlite connections should have been made with permanent private browsing enabled"
  );

  await maybeRemovePath(stagingPath);
  await maybeRemovePath(sourcePath);

  sandbox.restore();
});

/**
 * Test that the recover method correctly copies items from the recovery
 * directory into the destination profile directory.
 */
add_task(async function test_recover() {
  let cookiesBackupResource = new CookiesBackupResource();
  let recoveryPath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "CookiesBackupResource-recovery-test"
  );
  let destProfilePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "CookiesBackupResource-test-profile"
  );

  const simpleCopyFiles = [{ path: "cookies.sqlite" }];
  await createTestFiles(recoveryPath, simpleCopyFiles);

  // The backup method is expected to have returned a null ManifestEntry
  let postRecoveryEntry = await cookiesBackupResource.recover(
    null /* manifestEntry */,
    recoveryPath,
    destProfilePath
  );
  Assert.equal(
    postRecoveryEntry,
    null,
    "CookiesBackupResource.recover should return null as its post " +
      "recovery entry"
  );

  await assertFilesExist(destProfilePath, simpleCopyFiles);

  await maybeRemovePath(recoveryPath);
  await maybeRemovePath(destProfilePath);
});
