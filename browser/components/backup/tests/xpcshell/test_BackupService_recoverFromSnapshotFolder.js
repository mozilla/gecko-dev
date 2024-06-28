/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);
const { ArchiveUtils } = ChromeUtils.importESModule(
  "resource:///modules/backup/ArchiveUtils.sys.mjs"
);
const { JsonSchema } = ChromeUtils.importESModule(
  "resource://gre/modules/JsonSchema.sys.mjs"
);

/**
 * Tests that if the backup-manifest.json provides an appName different from
 * AppConstants.MOZ_APP_NAME of the currently running application, then
 * recoverFromSnapshotFolder should throw an exception.
 */
add_task(async function test_different_appName() {
  let testRecoveryPath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "testDifferentAppName"
  );

  let meta = Object.assign({}, FAKE_METADATA);
  meta.appName = "Some other application";
  Assert.notEqual(
    meta.appName,
    AppConstants.MOZ_APP_NAME,
    "Set up a different appName in the manifest correctly."
  );

  let manifest = {
    version: ArchiveUtils.SCHEMA_VERSION,
    meta,
    resources: {},
  };
  let schema = await BackupService.MANIFEST_SCHEMA;
  let validationResult = JsonSchema.validate(manifest, schema);
  Assert.ok(validationResult.valid, "Schema matches manifest");

  await IOUtils.writeJSON(
    PathUtils.join(testRecoveryPath, BackupService.MANIFEST_FILE_NAME),
    manifest
  );

  let bs = new BackupService();
  // This should reject and mention the invalid appName from the manifest.
  await Assert.rejects(
    bs.recoverFromSnapshotFolder(testRecoveryPath),
    new RegExp(`${meta.appName}`)
  );

  await IOUtils.remove(testRecoveryPath, { recursive: true });
});

/**
 * Tests that if the backup-manifest.json provides an appVersion greater than
 * AppConstants.MOZ_APP_VERSION of the currently running application, then
 * recoverFromSnapshotFolder should throw an exception.
 */
add_task(async function test_newer_appVersion() {
  let testRecoveryPath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "testNewerAppVersion"
  );

  let meta = Object.assign({}, FAKE_METADATA);
  // Hopefully this static version number will do for now.
  meta.appVersion = "999.0.0";
  Assert.equal(
    Services.vc.compare(AppConstants.MOZ_APP_VERSION, meta.appVersion),
    -1,
    "The current application version is less than 999.0.0."
  );

  let manifest = {
    version: ArchiveUtils.SCHEMA_VERSION,
    meta,
    resources: {},
  };
  let schema = await BackupService.MANIFEST_SCHEMA;
  let validationResult = JsonSchema.validate(manifest, schema);
  Assert.ok(validationResult.valid, "Schema matches manifest");

  await IOUtils.writeJSON(
    PathUtils.join(testRecoveryPath, BackupService.MANIFEST_FILE_NAME),
    manifest
  );

  let bs = new BackupService();
  // This should reject and mention the invalid appVersion from the manifest.
  await Assert.rejects(
    bs.recoverFromSnapshotFolder(testRecoveryPath),
    new RegExp(`${meta.appVersion}`)
  );

  await IOUtils.remove(testRecoveryPath, { recursive: true });
});
