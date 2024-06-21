/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This module expects to be able to load in both main-thread module contexts,
// as well as ChromeWorker contexts. Do not ChromeUtils.importESModule
// anything there at the top-level that's not compatible with both contexts.

export const ArchiveUtils = {
  /**
   * Convert an array containing only two bytes unsigned numbers to a base64
   * encoded string.
   *
   * @param {number[]} anArray
   *   The array that needs to be converted.
   * @returns {string}
   *   The string representation of the array.
   */
  arrayToBase64(anArray) {
    let result = "";
    let bytes = new Uint8Array(anArray);
    for (let i = 0; i < bytes.length; i++) {
      result += String.fromCharCode(bytes[i]);
    }
    return btoa(result);
  },

  /**
   * Convert a base64 encoded string to an Uint8Array.
   *
   * @param {string} base64Str
   *   The base64 encoded string that needs to be converted.
   * @returns {Uint8Array[]}
   *   The array representation of the string.
   */
  stringToArray(base64Str) {
    let binaryStr = atob(base64Str);
    let len = binaryStr.length;
    let bytes = new Uint8Array(len);
    for (let i = 0; i < len; i++) {
      bytes[i] = binaryStr.charCodeAt(i);
    }
    return bytes;
  },

  /**
   * The current shared schema version between the BackupManifest and the
   * ArchiveJSONBlock schemas.
   *
   * @type {number}
   */
  get SCHEMA_VERSION() {
    return 1;
  },

  /**
   * The version of the single-file archive that this version of the
   * application is expected to produce. Versions greater than this are not
   * interpretable by the application, and will cause an exception to be
   * thrown when loading the archive.
   *
   * Note: Until we can interpolate strings in our templates, changing this
   * value will require manual changes to the archive.template.html version
   * number in the header, as well as any test templates.
   *
   * @type {number}
   */
  get ARCHIVE_FILE_VERSION() {
    return 1;
  },

  /**
   * The HTML document comment start block, also indicating the start of the
   * inline MIME message block.
   *
   * @type {string}
   */
  get INLINE_MIME_START_MARKER() {
    return "<!-- Begin inline MIME --";
  },

  /**
   * The HTML document comment end block, also indicating the end of the
   * inline MIME message block.
   *
   * @type {string}
   */
  get INLINE_MIME_END_MARKER() {
    return "---- End inline MIME -->";
  },

  /**
   * The maximum number of bytes to read and encode when constructing the
   * single-file archive.
   *
   * @type {number}
   */
  get ARCHIVE_CHUNK_MAX_BYTES_SIZE() {
    return 1048576; // 2 ^ 20 bytes, per guidance from security engineering.
  },

  /**
   * @typedef {object} ComputeKeysResult
   * @property {Uint8Array} backupAuthKey
   *   The computed BackupAuthKey. This is returned as a Uint8Array because
   *   this key is used as a salt for other derived keys.
   * @property {CryptoKey} backupEncKey
   *   The computed BackupEncKey. This is an AES-GCM key used to encrypt and
   *   decrypt the secrets contained within a backup archive.
   */

  /**
   * Computes the BackupAuthKey and BackupEncKey from a recovery code and a
   * salt.
   *
   * @param {string} recoveryCode
   *   A recovery code. Callers are responsible for checking the length /
   *   entropy of the recovery code.
   * @param {Uint8Array} salt
   *   A salt that should be used for computing the keys.
   * @returns {ComputeKeysResult}
   */
  async computeBackupKeys(recoveryCode, salt) {
    let textEncoder = new TextEncoder();
    let recoveryCodeBytes = textEncoder.encode(recoveryCode);

    let keyMaterial = await crypto.subtle.importKey(
      "raw",
      recoveryCodeBytes,
      "PBKDF2",
      false /* extractable */,
      ["deriveBits"]
    );

    // Then we derive the "backup key", using
    // PBKDF2(recoveryCode, saltPrefix || SALT_SUFFIX, SHA-256, 600,000)
    const ITERATIONS = 600_000;

    let backupKeyBits = await crypto.subtle.deriveBits(
      {
        name: "PBKDF2",
        salt,
        iterations: ITERATIONS,
        hash: "SHA-256",
      },
      keyMaterial,
      256
    );

    // This is a little awkward, but the way that the WebCrypto API currently
    // works is that we have to read in those bits as a "raw HKDF key", and
    // only then can we derive our other HKDF keys from it.
    let backupKeyHKDF = await crypto.subtle.importKey(
      "raw",
      backupKeyBits,
      {
        name: "HKDF",
        hash: "SHA-256",
      },
      false /* extractable */,
      ["deriveKey", "deriveBits"]
    );

    // Re-derive BackupAuthKey as HKDF(backupKey, “backupkey-auth”, salt=None)
    let backupAuthKey = new Uint8Array(
      await crypto.subtle.deriveBits(
        {
          name: "HKDF",
          salt: new Uint8Array(0), // no salt
          info: textEncoder.encode("backupkey-auth"),
          hash: "SHA-256",
        },
        backupKeyHKDF,
        256
      )
    );

    let backupEncKey = await crypto.subtle.deriveKey(
      {
        name: "HKDF",
        salt: new Uint8Array(0), // no salt
        info: textEncoder.encode("backupkey-enc-key"),
        hash: "SHA-256",
      },
      backupKeyHKDF,
      { name: "AES-GCM", length: 256 },
      true /* extractable */,
      ["encrypt", "decrypt", "wrapKey"]
    );

    return { backupAuthKey, backupEncKey };
  },
};
