/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export const ERRORS = Object.freeze({
  /** User is not authorized to restore a backup archive */
  UNAUTHORIZED: 1,
  /** Selected backup archive can't be restored because it is corrupt */
  CORRUPTED_ARCHIVE: 2,
  /**
   * Selected backup archive can't be restored because the backup manifest
   * version is too old, from the future, or invalid
   */
  UNSUPPORTED_BACKUP_VERSION: 3,
  /** Backup service was not started or is not running */
  UNINITIALIZED: 4,
  /** Could not read from or write to the file system */
  FILE_SYSTEM_ERROR: 5,
  /** Encryption of backup archive failed */
  ENCRYPTION_FAILED: 6,
  /** Decryption of backup archive failed */
  DECRYPTION_FAILED: 7,
  /** Recovery of backup failed without a more specific cause */
  RECOVERY_FAILED: 8,
  /** Unknown error with backup system without a more specific cause */
  UNKNOWN: 9,
  /**
   * Backup system tried to enable backup encryption but it was
   * already enabled
   */
  ENCRYPTION_ALREADY_ENABLED: 10,
  /**
   * Backup system tried to disable backup encryption but it was
   * already disabled
   */
  ENCRYPTION_ALREADY_DISABLED: 11,
  /** User supplied a new password that is not a valid password */
  INVALID_PASSWORD: 12,
  /**
   * An error internal to the code that is likely caused by a bug
   * or other programmer error.
   */
  INTERNAL_ERROR: 13,
  /**
   * A backup cannot be recovered because the backup file was created
   * by a different application than the currently running application
   */
  UNSUPPORTED_APPLICATION: 14,
});

/**
 * These are steps that the BackupService or any of its subcomponents might
 * be going through during configuration, creation, deletion of or restoration
 * from a backup. This is used to provide extra information to our error
 * telemetry.
 */
export const STEPS = Object.freeze({
  /**
   * This is the initial step upon creating a backup before any other steps
   * begin.
   */
  CREATE_BACKUP_ENTRYPOINT: 1,

  /**
   * Determine the final destination for the written archive.
   */
  CREATE_BACKUP_RESOLVE_DESTINATION: 2,

  /**
   * Generate the manifest object for the backup.
   */
  CREATE_BACKUP_CREATE_MANIFEST: 3,

  /**
   * Create the main `backups` working directory in the profile directory if it
   * doesn't already exist.
   */
  CREATE_BACKUP_CREATE_BACKUPS_FOLDER: 4,

  /**
   * Create the staging directory for the backup.
   */
  CREATE_BACKUP_CREATE_STAGING_FOLDER: 5,

  /**
   * Attempt to load the encryption state if one exists.
   */
  CREATE_BACKUP_LOAD_ENCSTATE: 6,

  /**
   * Run the backup routine for each BackupResource.
   */
  CREATE_BACKUP_RUN_BACKUP: 7,

  /**
   * After populating with the data from each BackupResource, verify that
   * the manifest adheres to the BackupManifest schema.
   */
  CREATE_BACKUP_VERIFY_MANIFEST: 8,

  /**
   * Write the backup manifest to the staging directory.
   */
  CREATE_BACKUP_WRITE_MANIFEST: 9,

  /**
   * Rename the staging directory with the time code, and clear out any
   * expired directories.
   */
  CREATE_BACKUP_FINALIZE_STAGING: 10,

  /**
   * Compress the staging directory into a single file.
   */
  CREATE_BACKUP_COMPRESS_STAGING: 11,

  /**
   * Generate the single-file archive.
   */
  CREATE_BACKUP_CREATE_ARCHIVE: 12,

  /**
   * Finalize the single-file archive and move it into the destination
   * directory.
   */
  CREATE_BACKUP_FINALIZE_ARCHIVE: 13,
});
