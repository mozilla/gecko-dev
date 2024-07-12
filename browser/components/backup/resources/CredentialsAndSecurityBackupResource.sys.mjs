/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

import { BackupResource } from "resource:///modules/backup/BackupResource.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BackupService: "resource:///modules/backup/BackupService.sys.mjs",
  OSKeyStore: "resource://gre/modules/OSKeyStore.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "nativeOSKeyStore",
  "@mozilla.org/security/oskeystore;1",
  Ci.nsIOSKeyStore
);

/**
 * Class representing files needed for logins, payment methods and form autofill within a user profile.
 */
export class CredentialsAndSecurityBackupResource extends BackupResource {
  static get key() {
    return "credentials_and_security";
  }

  static get requiresEncryption() {
    return true;
  }

  async backup(
    stagingPath,
    profilePath = PathUtils.profileDir,
    _isEncrypting = false
  ) {
    const simpleCopyFiles = [
      "pkcs11.txt",
      "logins.json",
      "logins-backup.json",
      "autofill-profiles.json",
    ];
    await BackupResource.copyFiles(profilePath, stagingPath, simpleCopyFiles);

    const sqliteDatabases = ["cert9.db", "key4.db", "credentialstate.sqlite"];
    await BackupResource.copySqliteDatabases(
      profilePath,
      stagingPath,
      sqliteDatabases
    );

    return null;
  }

  async recover(_manifestEntry, recoveryPath, destProfilePath) {
    // Payment methods would have been encrypted via OSKeyStore, which might
    // have a different OSKeyStore secret than this profile (particularly if
    // we're on a different machine).
    //
    // BackupService created a temporary native OSKeyStore that we can use
    // to decrypt the payment methods using the old secret used at backup
    // time. We should then re-encrypt those with the current OSKeyStore.
    const AUTOFILL_RECORDS_PATH = PathUtils.join(
      recoveryPath,
      "autofill-profiles.json"
    );
    let autofillRecords = await IOUtils.readJSON(AUTOFILL_RECORDS_PATH);

    for (let creditCard of autofillRecords.creditCards) {
      let oldEncryptedCard = creditCard["cc-number-encrypted"];
      if (oldEncryptedCard) {
        // We use the native OSKeyStore backend to decrypt the bytes with the
        // original secret in order to skip authentication dialogs.
        let plaintextCardBytes = await lazy.nativeOSKeyStore.asyncDecryptBytes(
          lazy.BackupService.RECOVERY_OSKEYSTORE_LABEL,
          oldEncryptedCard
        );
        let plaintextCard = String.fromCharCode.apply(
          String,
          plaintextCardBytes
        );
        // We're accessing the "real" OSKeyStore for this device here, and
        // encrypting the card with it.
        let newEncryptedCard = await lazy.OSKeyStore.encrypt(plaintextCard);
        creditCard["cc-number-encrypted"] = newEncryptedCard;
      }
    }

    await IOUtils.writeJSON(AUTOFILL_RECORDS_PATH, autofillRecords);

    const files = [
      "pkcs11.txt",
      "logins.json",
      "logins-backup.json",
      "autofill-profiles.json",
      "cert9.db",
      "key4.db",
      "credentialstate.sqlite",
    ];
    await BackupResource.copyFiles(recoveryPath, destProfilePath, files);

    return null;
  }

  async measure(profilePath = PathUtils.profileDir) {
    const securityFiles = ["cert9.db", "pkcs11.txt"];
    let securitySize = 0;

    for (let filePath of securityFiles) {
      let resourcePath = PathUtils.join(profilePath, filePath);
      let resourceSize = await BackupResource.getFileSize(resourcePath);
      if (Number.isInteger(resourceSize)) {
        securitySize += resourceSize;
      }
    }

    Glean.browserBackup.securityDataSize.set(securitySize);

    const credentialsFiles = [
      "key4.db",
      "logins.json",
      "logins-backup.json",
      "autofill-profiles.json",
      "credentialstate.sqlite",
    ];
    let credentialsSize = 0;

    for (let filePath of credentialsFiles) {
      let resourcePath = PathUtils.join(profilePath, filePath);
      let resourceSize = await BackupResource.getFileSize(resourcePath);
      if (Number.isInteger(resourceSize)) {
        credentialsSize += resourceSize;
      }
    }

    Glean.browserBackup.credentialsDataSize.set(credentialsSize);
  }
}
