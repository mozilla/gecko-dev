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
};
