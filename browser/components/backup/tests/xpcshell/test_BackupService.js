/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);
const { JsonSchema } = ChromeUtils.importESModule(
  "resource://gre/modules/JsonSchema.sys.mjs"
);
const { UIState } = ChromeUtils.importESModule(
  "resource://services-sync/UIState.sys.mjs"
);
const { ClientID } = ChromeUtils.importESModule(
  "resource://gre/modules/ClientID.sys.mjs"
);

/** @type {nsIToolkitProfile} */
let currentProfile;

add_setup(function () {
  // FOG needs to be initialized in order for data to flow.
  Services.fog.initializeFOG();

  // Much of this setup is copied from toolkit/profile/xpcshell/head.js. It is
  // needed in order to put the xpcshell test environment into the state where
  // it thinks its profile is the one pointed at by
  // nsIToolkitProfileService.currentProfile.
  let gProfD = do_get_profile();
  let gDataHome = gProfD.clone();
  gDataHome.append("data");
  gDataHome.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  let gDataHomeLocal = gProfD.clone();
  gDataHomeLocal.append("local");
  gDataHomeLocal.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0o755);

  let xreDirProvider = Cc["@mozilla.org/xre/directory-provider;1"].getService(
    Ci.nsIXREDirProvider
  );
  xreDirProvider.setUserDataDirectory(gDataHome, false);
  xreDirProvider.setUserDataDirectory(gDataHomeLocal, true);

  let profileSvc = Cc["@mozilla.org/toolkit/profile-service;1"].getService(
    Ci.nsIToolkitProfileService
  );

  let createdProfile = {};
  let didCreate = profileSvc.selectStartupProfile(
    ["xpcshell"],
    false,
    AppConstants.UPDATE_CHANNEL,
    "",
    {},
    {},
    createdProfile
  );
  Assert.ok(didCreate, "Created a testing profile and set it to current.");
  Assert.equal(
    profileSvc.currentProfile,
    createdProfile.value,
    "Profile set to current"
  );

  currentProfile = createdProfile.value;
});

/**
 * A utility function for testing BackupService.createBackup. This helper
 * function:
 *
 * 1. Produces a backup of fake resources
 * 2. Recovers the backup into a new profile directory
 * 3. Ensures that the resources had their backup/recovery methods called
 *
 * @param {object} sandbox
 *   The Sinon sandbox to be used stubs and mocks. The test using this helper
 *   is responsible for creating and resetting this sandbox.
 * @param {function(BackupManifest): void} taskFn
 *   A function that is run once all default checks are done.
 *   After this function returns, all resources will be cleaned up.
 * @returns {Promise<undefined>}
 */
async function testCreateBackupHelper(sandbox, taskFn) {
  Services.fog.testResetFOG();
  // Handle for the metric for total byte size of staging folder
  let totalBackupSizeHistogram = TelemetryTestUtils.getAndClearHistogram(
    "BROWSER_BACKUP_TOTAL_BACKUP_SIZE"
  );
  // Handle for the metric for total time taking by profile backup
  let backupTimerHistogram = TelemetryTestUtils.getAndClearHistogram(
    "BROWSER_BACKUP_TOTAL_BACKUP_TIME_MS"
  );

  const EXPECTED_CLIENT_ID = await ClientID.getClientID();

  let fake1ManifestEntry = { fake1: "hello from 1" };
  sandbox
    .stub(FakeBackupResource1.prototype, "backup")
    .resolves(fake1ManifestEntry);
  sandbox.stub(FakeBackupResource1.prototype, "recover").resolves();

  sandbox
    .stub(FakeBackupResource2.prototype, "backup")
    .rejects(new Error("Some failure to backup"));
  sandbox.stub(FakeBackupResource2.prototype, "recover");

  let fake3ManifestEntry = { fake3: "hello from 3" };
  let fake3PostRecoveryEntry = { someData: "hello again from 3" };
  sandbox
    .stub(FakeBackupResource3.prototype, "backup")
    .resolves(fake3ManifestEntry);
  sandbox
    .stub(FakeBackupResource3.prototype, "recover")
    .resolves(fake3PostRecoveryEntry);

  let bs = new BackupService({
    FakeBackupResource1,
    FakeBackupResource2,
    FakeBackupResource3,
  });

  let fakeProfilePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "createBackupTest"
  );

  let testTelemetryStateObject = {
    clientID: "ed209123-04a1-04a1-04a1-c0ffeec0ffee",
  };
  await IOUtils.writeJSON(
    PathUtils.join(PathUtils.profileDir, "datareporting", "state.json"),
    testTelemetryStateObject
  );

  Assert.ok(!bs.state.lastBackupDate, "No backup date is stored in state.");
  let { manifest, archivePath: backupFilePath } = await bs.createBackup({
    profilePath: fakeProfilePath,
  });
  Assert.ok(bs.state.lastBackupDate, "The backup date was recorded.");

  // Validate total backup time metrics were recorded
  assertSingleTimeMeasurement(
    Glean.browserBackup.totalBackupTime.testGetValue()
  );
  assertHistogramMeasurementQuantity(
    backupTimerHistogram,
    1,
    "Should have collected a single measurement for total backup time"
  );

  Assert.ok(await IOUtils.exists(backupFilePath), "The backup file exists");

  let archiveDateSuffix = bs.generateArchiveDateSuffix(
    new Date(manifest.meta.date)
  );

  // We also expect the HTML file to have been written to the folder pointed
  // at by browser.backups.location, within backupDirPath folder.
  const EXPECTED_ARCHIVE_PATH = PathUtils.join(
    bs.state.backupDirPath,
    `${BackupService.BACKUP_FILE_NAME}_${manifest.meta.profileName}_${archiveDateSuffix}.html`
  );
  Assert.ok(
    await IOUtils.exists(EXPECTED_ARCHIVE_PATH),
    "Single-file backup archive was written."
  );
  Assert.equal(
    backupFilePath,
    EXPECTED_ARCHIVE_PATH,
    "Backup was written to the configured destination folder"
  );

  let snapshotsDirectoryPath = PathUtils.join(
    fakeProfilePath,
    BackupService.PROFILE_FOLDER_NAME,
    BackupService.SNAPSHOTS_FOLDER_NAME
  );
  let snapshotsDirectoryContentsPaths = await IOUtils.getChildren(
    snapshotsDirectoryPath
  );
  let snapshotsDirectoryContents = await Promise.all(
    snapshotsDirectoryContentsPaths.map(IOUtils.stat)
  );
  let snapshotsDirectorySubdirectories = snapshotsDirectoryContents.filter(
    file => file.type === "directory"
  );
  Assert.equal(
    snapshotsDirectorySubdirectories.length,
    0,
    "Snapshots directory should have had all staging folders cleaned up"
  );

  // 1 mebibyte minimum recorded value if staging folder is under 1 mebibyte
  // This assumes that these BackupService tests do not create sizable fake files
  const SMALLEST_BACKUP_SIZE_BYTES = 1048576;
  const SMALLEST_BACKUP_SIZE_MEBIBYTES = 1;

  let totalBackupSize = Glean.browserBackup.totalBackupSize.testGetValue();
  Assert.equal(
    totalBackupSize.count,
    1,
    "Should have collected a single measurement for the total backup size"
  );
  Assert.equal(
    totalBackupSize.sum,
    SMALLEST_BACKUP_SIZE_BYTES,
    "Should have collected the right value for the total backup size"
  );
  TelemetryTestUtils.assertHistogram(
    totalBackupSizeHistogram,
    SMALLEST_BACKUP_SIZE_MEBIBYTES,
    1
  );

  // Check that resources were called from highest to lowest backup priority.
  sinon.assert.callOrder(
    FakeBackupResource3.prototype.backup,
    FakeBackupResource2.prototype.backup,
    FakeBackupResource1.prototype.backup
  );

  let schema = await BackupService.MANIFEST_SCHEMA;
  let validationResult = JsonSchema.validate(manifest, schema);
  Assert.ok(validationResult.valid, "Schema matches manifest");
  Assert.deepEqual(
    Object.keys(manifest.resources).sort(),
    ["fake1", "fake3"],
    "Manifest contains all expected BackupResource keys"
  );
  Assert.deepEqual(
    manifest.resources.fake1,
    fake1ManifestEntry,
    "Manifest contains the expected entry for FakeBackupResource1"
  );
  Assert.deepEqual(
    manifest.resources.fake3,
    fake3ManifestEntry,
    "Manifest contains the expected entry for FakeBackupResource3"
  );
  Assert.equal(
    manifest.meta.legacyClientID,
    EXPECTED_CLIENT_ID,
    "The client ID was stored properly."
  );
  Assert.equal(
    manifest.meta.profileName,
    currentProfile.name,
    "The profile name was stored properly"
  );

  let recoveredProfilePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "createBackupTestRecoveredProfile"
  );

  let recoveredProfile = await bs.recoverFromBackupArchive(
    backupFilePath,
    null,
    false,
    fakeProfilePath,
    recoveredProfilePath
  );

  Assert.ok(
    recoveredProfile.name.startsWith(currentProfile.name),
    "Should maintain profile name across backup and restore"
  );

  // Check that resources were recovered from highest to lowest backup priority.
  sinon.assert.callOrder(
    FakeBackupResource3.prototype.recover,
    FakeBackupResource1.prototype.recover
  );

  let postRecoveryFilePath = PathUtils.join(
    recoveredProfilePath,
    BackupService.POST_RECOVERY_FILE_NAME
  );
  Assert.ok(
    await IOUtils.exists(postRecoveryFilePath),
    "Should have created post-recovery data file"
  );
  let postRecoveryData = await IOUtils.readJSON(postRecoveryFilePath);
  Assert.deepEqual(
    postRecoveryData.fake3,
    fake3PostRecoveryEntry,
    "Should have post-recovery data from fake backup 3"
  );

  let newProfileTelemetryStateObject = await IOUtils.readJSON(
    PathUtils.join(recoveredProfilePath, "datareporting", "state.json")
  );
  Assert.deepEqual(
    testTelemetryStateObject,
    newProfileTelemetryStateObject,
    "Recovered profile inherited telemetry state from the profile that " +
      "initiated recovery"
  );

  taskFn(manifest);

  await maybeRemovePath(backupFilePath);
  await maybeRemovePath(fakeProfilePath);
  await maybeRemovePath(recoveredProfilePath);
  await maybeRemovePath(EXPECTED_ARCHIVE_PATH);
}

/**
 * Tests that calling BackupService.createBackup will call backup on each
 * registered BackupResource, and that each BackupResource will have a folder
 * created for them to write into. Tests in the signed-out state.
 */
add_task(async function test_createBackup_signed_out() {
  let sandbox = sinon.createSandbox();

  sandbox
    .stub(UIState, "get")
    .returns({ status: UIState.STATUS_NOT_CONFIGURED });
  await testCreateBackupHelper(sandbox, manifest => {
    Assert.equal(
      manifest.meta.accountID,
      undefined,
      "Account ID should be undefined."
    );
    Assert.equal(
      manifest.meta.accountEmail,
      undefined,
      "Account email should be undefined."
    );
  });

  sandbox.restore();
});

/**
 * Tests that calling BackupService.createBackup will call backup on each
 * registered BackupResource, and that each BackupResource will have a folder
 * created for them to write into. Tests in the signed-in state.
 */
add_task(async function test_createBackup_signed_in() {
  let sandbox = sinon.createSandbox();

  const TEST_UID = "ThisIsMyTestUID";
  const TEST_EMAIL = "foxy@mozilla.org";

  sandbox.stub(UIState, "get").returns({
    status: UIState.STATUS_SIGNED_IN,
    uid: TEST_UID,
    email: TEST_EMAIL,
  });

  await testCreateBackupHelper(sandbox, manifest => {
    Assert.equal(
      manifest.meta.accountID,
      TEST_UID,
      "Account ID should be set properly."
    );
    Assert.equal(
      manifest.meta.accountEmail,
      TEST_EMAIL,
      "Account email should be set properly."
    );
  });

  sandbox.restore();
});

/**
 * Tests that any internal file system errors in BackupService.createBackup
 * do not bubble up any errors.
 */
add_task(
  {
    // Bug 1905724 - Need to find a way to deny write access to backup directory on Windows
    skip_if: () => AppConstants.platform == "win",
  },
  async function test_createBackup_robustToFileSystemErrors() {
    let sandbox = sinon.createSandbox();
    Services.fog.testResetFOG();
    // Handle for the metric for total time taking by profile backup
    let backupTimerHistogram = TelemetryTestUtils.getAndClearHistogram(
      "BROWSER_BACKUP_TOTAL_BACKUP_TIME_MS"
    );

    const TEST_UID = "ThisIsMyTestUID";
    const TEST_EMAIL = "foxy@mozilla.org";

    sandbox.stub(UIState, "get").returns({
      status: UIState.STATUS_SIGNED_IN,
      uid: TEST_UID,
      email: TEST_EMAIL,
    });

    // Create a read-only fake profile folder to which the backup service
    // won't be able to make writes
    let inaccessibleProfilePath = await IOUtils.createUniqueDirectory(
      PathUtils.tempDir,
      "createBackupErrorInaccessible"
    );
    IOUtils.setPermissions(inaccessibleProfilePath, 0o444);

    const bs = new BackupService({});

    await bs
      .createBackup({ profilePath: inaccessibleProfilePath })
      .then(result => {
        Assert.equal(result, null, "Should return null on error");

        // Validate total backup time metrics were recorded
        const totalBackupTime =
          Glean.browserBackup.totalBackupTime.testGetValue();
        Assert.equal(
          totalBackupTime,
          null,
          "Should not have measured total backup time for failed backup"
        );
        assertHistogramMeasurementQuantity(backupTimerHistogram, 0);
      })
      .catch(() => {
        // Trigger failure if there was an uncaught error
        Assert.ok(false, "Should not have bubbled up an error");
      })
      .finally(async () => {
        await IOUtils.remove(inaccessibleProfilePath, { recursive: true });
        sandbox.restore();
      });
  }
);

/**
 * Creates a directory that looks a lot like a decompressed backup archive,
 * and then tests that BackupService.recoverFromSnapshotFolder can create a new
 * profile and recover into it.
 */
add_task(async function test_recoverFromSnapshotFolder() {
  let sandbox = sinon.createSandbox();
  let fakeEntryMap = new Map();
  let backupResourceClasses = [
    FakeBackupResource1,
    FakeBackupResource2,
    FakeBackupResource3,
  ];

  let i = 1;
  for (let backupResourceClass of backupResourceClasses) {
    let fakeManifestEntry = { [`fake${i}`]: `hello from backup - ${i}` };
    sandbox
      .stub(backupResourceClass.prototype, "backup")
      .resolves(fakeManifestEntry);

    let fakePostRecoveryEntry = { [`fake${i}`]: `hello from recover - ${i}` };
    sandbox
      .stub(backupResourceClass.prototype, "recover")
      .resolves(fakePostRecoveryEntry);

    fakeEntryMap.set(backupResourceClass, {
      manifestEntry: fakeManifestEntry,
      postRecoveryEntry: fakePostRecoveryEntry,
    });

    ++i;
  }

  let bs = new BackupService({
    FakeBackupResource1,
    FakeBackupResource2,
    FakeBackupResource3,
  });

  let oldProfilePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "recoverFromSnapshotFolderTest"
  );
  let newProfileRootPath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "recoverFromSnapshotFolderTest-newProfileRoot"
  );

  let { stagingPath } = await bs.createBackup({ profilePath: oldProfilePath });

  // Ensure that the appName in the written manifest matches the current
  // MOZ_APP_NAME.
  let manifest = await IOUtils.readJSON(
    PathUtils.join(stagingPath, BackupService.MANIFEST_FILE_NAME)
  );
  Assert.equal(
    manifest.meta.appName,
    AppConstants.MOZ_APP_NAME,
    "appName matches MOZ_APP_NAME"
  );
  // And that appVersion matches MOZ_APP_VERSION
  Assert.equal(
    manifest.meta.appVersion,
    AppConstants.MOZ_APP_VERSION,
    "appVersion matches MOZ_APP_VERSION"
  );

  let testTelemetryStateObject = {
    clientID: "ed209123-04a1-04a1-04a1-c0ffeec0ffee",
  };
  await IOUtils.writeJSON(
    PathUtils.join(PathUtils.profileDir, "datareporting", "state.json"),
    testTelemetryStateObject
  );

  let profile = await bs.recoverFromSnapshotFolder(
    stagingPath,
    false /* shouldLaunch */,
    newProfileRootPath,
    null /* encState */
  );
  Assert.ok(profile, "An nsIToolkitProfile was created.");
  let newProfilePath = profile.rootDir.path;

  let postRecoveryFilePath = PathUtils.join(
    newProfilePath,
    "post-recovery.json"
  );
  let postRecovery = await IOUtils.readJSON(postRecoveryFilePath);

  for (let backupResourceClass of backupResourceClasses) {
    let expectedResourceFolder = PathUtils.join(
      stagingPath,
      backupResourceClass.key
    );

    let { manifestEntry, postRecoveryEntry } =
      fakeEntryMap.get(backupResourceClass);

    Assert.ok(
      backupResourceClass.prototype.recover.calledOnce,
      `Recover was called for ${backupResourceClass.key}`
    );
    Assert.ok(
      backupResourceClass.prototype.recover.calledWith(
        manifestEntry,
        expectedResourceFolder,
        newProfilePath
      ),
      `Recover was passed the right arguments for ${backupResourceClass.key}`
    );
    Assert.deepEqual(
      postRecoveryEntry,
      postRecovery[backupResourceClass.key],
      "The post recovery data is as expected"
    );
  }

  let newProfileTelemetryStateObject = await IOUtils.readJSON(
    PathUtils.join(newProfileRootPath, "datareporting", "state.json")
  );
  Assert.deepEqual(
    testTelemetryStateObject,
    newProfileTelemetryStateObject,
    "Recovered profile inherited telemetry state from the profile that " +
      "initiated recovery"
  );

  await IOUtils.remove(oldProfilePath, { recursive: true });
  await IOUtils.remove(newProfileRootPath, { recursive: true });
  sandbox.restore();
});

/**
 * Tests that if there's a post-recovery.json file in the profile directory
 * when checkForPostRecovery() is called, that it is processed, and the
 * postRecovery methods on the associated BackupResources are called with the
 * entry values from the file.
 */
add_task(async function test_checkForPostRecovery() {
  let sandbox = sinon.createSandbox();

  let testProfilePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "checkForPostRecoveryTest"
  );
  let fakePostRecoveryObject = {
    [FakeBackupResource1.key]: "test 1",
    [FakeBackupResource3.key]: "test 3",
  };
  await IOUtils.writeJSON(
    PathUtils.join(testProfilePath, BackupService.POST_RECOVERY_FILE_NAME),
    fakePostRecoveryObject
  );

  sandbox.stub(FakeBackupResource1.prototype, "postRecovery").resolves();
  sandbox.stub(FakeBackupResource2.prototype, "postRecovery").resolves();
  sandbox.stub(FakeBackupResource3.prototype, "postRecovery").resolves();

  let bs = new BackupService({
    FakeBackupResource1,
    FakeBackupResource2,
    FakeBackupResource3,
  });

  await bs.checkForPostRecovery(testProfilePath);
  await bs.postRecoveryComplete;

  Assert.ok(
    FakeBackupResource1.prototype.postRecovery.calledOnce,
    "FakeBackupResource1.postRecovery was called once"
  );
  Assert.ok(
    FakeBackupResource2.prototype.postRecovery.notCalled,
    "FakeBackupResource2.postRecovery was not called"
  );
  Assert.ok(
    FakeBackupResource3.prototype.postRecovery.calledOnce,
    "FakeBackupResource3.postRecovery was called once"
  );
  Assert.ok(
    FakeBackupResource1.prototype.postRecovery.calledWith(
      fakePostRecoveryObject[FakeBackupResource1.key]
    ),
    "FakeBackupResource1.postRecovery was called with the expected argument"
  );
  Assert.ok(
    FakeBackupResource3.prototype.postRecovery.calledWith(
      fakePostRecoveryObject[FakeBackupResource3.key]
    ),
    "FakeBackupResource3.postRecovery was called with the expected argument"
  );

  await IOUtils.remove(testProfilePath, { recursive: true });
  sandbox.restore();
});

/**
 * Tests that getBackupFileInfo updates backupFileInfo in the state with a subset
 * of info from the fake SampleArchiveResult returned by sampleArchive().
 */
add_task(async function test_getBackupFileInfo() {
  let sandbox = sinon.createSandbox();

  const DATE = "2024-06-25T21:59:11.777Z";
  const IS_ENCRYPTED = true;

  let fakeSampleArchiveResult = {
    isEncrypted: IS_ENCRYPTED,
    startByteOffset: 26985,
    contentType: "multipart/mixed",
    archiveJSON: { version: 1, meta: { date: DATE }, encConfig: {} },
  };

  sandbox
    .stub(BackupService.prototype, "sampleArchive")
    .resolves(fakeSampleArchiveResult);

  let bs = new BackupService();

  await bs.getBackupFileInfo("fake-archive.html");

  Assert.ok(
    BackupService.prototype.sampleArchive.calledOnce,
    "sampleArchive was called once"
  );

  Assert.deepEqual(
    bs.state.backupFileInfo,
    { isEncrypted: IS_ENCRYPTED, date: DATE },
    "State should match a subset from the archive sample."
  );

  sandbox.restore();
});
