/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ArchiveUtils } = ChromeUtils.importESModule(
  "resource:///modules/backup/ArchiveUtils.sys.mjs"
);

/**
 * Tests that there is a BackupManifest schema and a ArchiveJSONBlock schema
 * for each schema version from version 1 to ArchiveUtils.SCHEMA_VERSION. This
 * test assumes that subsequent schemas versions are only going to increase one
 * version at a time, which seems like a reasonable assumption.
 */

add_task(async function test_schemas_exist() {
  for (let version = 1; version <= ArchiveUtils.SCHEMA_VERSION; ++version) {
    let manifestSchema = await BackupService.getSchemaForVersion(
      BackupService.SCHEMAS.BACKUP_MANIFEST,
      version
    );
    Assert.ok(
      manifestSchema,
      `The BackupManifest schema exists for version ${version}`
    );
    let archiveJSONBlockSchema = await BackupService.getSchemaForVersion(
      BackupService.SCHEMAS.ARCHIVE_JSON_BLOCK,
      version
    );
    Assert.ok(
      archiveJSONBlockSchema,
      `The ArchiveJSONBlock schema exists for version ${version}`
    );
  }
});
