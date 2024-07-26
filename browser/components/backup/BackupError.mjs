/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Error class with specific backup-related error causes.
 *
 * Can be serialized and deserialized across a worker boundary using
 * the BasePromiseWorker and PromiseWorker machinery in this codebase.
 *
 * @see PromiseWorker.mjs
 * @see PromiseWorker.sys.mjs
 */
export class BackupError extends Error {
  name = "BackupError";

  /**
   * @param {string} message
   *   Error message
   * @param {number} cause
   *   Error cause code from backup-constants.mjs:ERRORS
   */
  constructor(message, cause) {
    super(message, { cause });
  }
  /**
   * @typedef {object} SerializedBackupError
   * @property {'BackupError'} exn
   *   Exception name for PromiseWorker serialization
   * @property {string} message
   *   Error message
   * @property {number} cause
   *   Error cause code from backup-constants.mjs:ERRORS
   * @property {string} stack
   *   Stack trace of the error
   */

  /**
   * Used by PromiseWorker.mjs from within a web worker in order to
   * serialize this error for later reconstruction in the main process.
   *
   * @returns {SerializedBackupError}
   * @see PromiseWorker.mjs
   */
  toMsg() {
    return {
      exn: BackupError.name,
      message: this.message,
      cause: this.cause,
      stack: this.stack,
    };
  }

  /**
   * @param {SerializedBackupError} serialized
   *   Worker error serialized by PromiseWorker
   * @returns {BackupError}
   */
  static fromMsg(serialized) {
    let error = new BackupError(serialized.message, serialized.cause);
    error.stack = serialized.stack;
    return error;
  }
}
