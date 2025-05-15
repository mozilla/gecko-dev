/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Handles serialization of the data and persistence into a file.
 *
 * This modules handles the raw data stored in JavaScript serializable objects,
 * and contains no special validation or query logic, that is handled entirely
 * by "storage.js" instead.
 *
 * The data can be manipulated only after it has been loaded from disk.  The
 * load process can happen asynchronously, through the "load" method, or
 * synchronously, through "ensureDataReady".  After any modification, the
 * "saveSoon" method must be called to flush the data to disk asynchronously.
 *
 * The raw data should be manipulated synchronously, without waiting for the
 * event loop or for promise resolution, so that the saved file is always
 * consistent.  This synchronous approach also simplifies the query and update
 * logic.  For example, it is possible to find an object and modify it
 * immediately without caring whether other code modifies it in the meantime.
 *
 * An asynchronous shutdown observer makes sure that data is always saved before
 * the browser is closed. The data cannot be modified during shutdown.
 *
 * The file is stored in JSON format, without indentation, using UTF-8 encoding.
 */

// Globals

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  DeferredTask: "resource://gre/modules/DeferredTask.sys.mjs",
  FileUtils: "resource://gre/modules/FileUtils.sys.mjs",
  NetUtil: "resource://gre/modules/NetUtil.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "gTextDecoder", function () {
  return new TextDecoder();
});

const FileInputStream = Components.Constructor(
  "@mozilla.org/network/file-input-stream;1",
  "nsIFileInputStream",
  "init"
);

/**
 * Delay between a change to the data and the related save operation.
 */
const kSaveDelayMs = 1500;

// JSONFile

/**
 * Handles serialization of the data and persistence into a file.
 *
 * @param {Object} config An object containing following members:
 * @param {string} config.path
 *   String containing the file path where data should be saved.
 * @param {string} [config.sanitizedBasename]
 *   Sanitized string identifier used for logging,
 *   shutdown debugging, and telemetry. Defaults to basename of given `path`,
 *   sanitized.
 * @param {Function} [config.dataPostProcessor]
 *   Function triggered when data is just loaded. The data object will be passed
 *   as the first argument and should be returned no matter if it's modified or
 *   not. Its failure leads to the failure of load() and ensureDataReady().
 * @param {number} [config.saveDelayMs]
 *   Number indicating the delay (in milliseconds) between a change to the data
 *   and the related save operation. The default value will be applied if
 *   omitted.
 * @param {Function} [config.beforeSave]
 *   Promise-returning function triggered just before the data is written to
 *   disk. This can be used to create any intermediate directories before
 *   saving. The file will not be saved if the promise rejects or the function
 *   throws an exception.
 * @param {nsIAsyncShutdownClient} [config.finalizeAt]
 *   An `IOUtils` phase or barrier client that should automatically finalize the
 *   file when triggered. Defaults to `profileBeforeChange`; exposed as an
 *   option for testing.
 * @param {string} [config.compression]
 *   A compression algorithm to use when reading and writing the data.
 * @param {string} [config.backupTo]
 *   A string value indicating where writeAtomic should create a backup before
 *   writing to json files. Note that using this option currently ensures that
 *   we automatically restore backed up json files in load() and
 *   ensureDataReady() when original files are missing or corrupt.
 * @param {Function} [config.saveFailureHandler]
 *   A synchronous function that will be called if saving the data object ever
 *   causes an exception to be thrown (and toJSONSafe is not implemented on
 *   data).
 */
export function JSONFile(config) {
  this.path = config.path;
  this.sanitizedBasename =
    config.sanitizedBasename ??
    PathUtils.filename(this.path)
      .replace(/\.json(.lz4)?$/, "")
      .replaceAll(/[^a-zA-Z0-9_.]/g, "");

  if (typeof config.dataPostProcessor === "function") {
    this._dataPostProcessor = config.dataPostProcessor;
  }
  if (typeof config.beforeSave === "function") {
    this._beforeSave = config.beforeSave;
  }

  if (config.saveDelayMs === undefined) {
    config.saveDelayMs = kSaveDelayMs;
  }
  this._saver = new lazy.DeferredTask(() => this._save(), config.saveDelayMs);

  this._options = {};
  if (config.compression) {
    this._options.decompress = this._options.compress = true;
  }

  if (config.backupTo) {
    this._options.backupFile = this._options.backupTo = config.backupTo;
  }

  if (config.saveFailureHandler) {
    this._saveFailureHandler = config.saveFailureHandler;
  }

  this._finalizeAt = config.finalizeAt || IOUtils.profileBeforeChange;
  this._finalizeInternalBound = this._finalizeInternal.bind(this);
  this._finalizeAt.addBlocker(
    `JSON store: writing data for '${this.sanitizedBasename}'`,
    this._finalizeInternalBound,
    () => ({ sanitizedBasename: this.sanitizedBasename })
  );
}

JSONFile.prototype = {
  /**
   * String containing the file path where data should be saved.
   */
  path: "",

  /**
   * Sanitized identifier used for logging, shutdown debugging, and telemetry.
   */
  sanitizedBasename: "",

  /**
   * True when data has been loaded.
   */
  dataReady: false,

  /**
   * DeferredTask that handles the save operation.
   */
  _saver: null,

  /**
   * A function that will be called if saving the data results in an exception
   * being thrown.
   */
  _saveFailureHandler: () => {},

  /**
   * Internal data object.
   */
  _data: null,

  /**
   * Internal fields used during finalization.
   */
  _finalizeAt: null,
  _finalizePromise: null,
  _finalizeInternalBound: null,

  /**
   * Serializable object containing the data. This is populated directly with
   * the data loaded from the file, and is saved without modifications.
   *
   * The raw data should be manipulated synchronously, without waiting for the
   * event loop or for promise resolution, so that the saved file is always
   * consistent.
   */
  get data() {
    if (!this.dataReady) {
      throw new Error("Data is not ready.");
    }
    return this._data;
  },

  /**
   * Sets the loaded data to a new object. This will overwrite any persisted
   * data on the next save.
   */
  set data(data) {
    this._data = data;
    this.dataReady = true;
  },

  /**
   * Loads persistent data from the file to memory.
   *
   * @return {Promise}
   * @resolves When the operation finished successfully.
   * @rejects JavaScript exception when dataPostProcessor fails. It never fails
   *          if there is no dataPostProcessor.
   */
  async load() {
    if (this.dataReady) {
      return;
    }

    let data = {};

    try {
      data = await IOUtils.readJSON(this.path, this._options);

      // If synchronous loading happened in the meantime, exit now.
      if (this.dataReady) {
        return;
      }
    } catch (ex) {
      // If an exception occurs because the file does not exist or it cannot be read,
      // we do two things.
      // 1. For consumers of JSONFile.sys.mjs that have configured a `backupTo` path option,
      //    we try to look for and use backed up json files first. If the backup
      //    is also not found or if the backup is unreadable, we then start with an empty file.
      // 2. If a consumer does not configure a `backupTo` path option, we just start
      //    with an empty file.

      // In the event that the file exists, but an exception is thrown because it cannot be read,
      // we store it as a .corrupt file for debugging purposes.

      let errorNo = ex.winLastError || ex.unixErrno;
      this._recordTelemetry(errorNo ? errorNo.toString() : "");
      if (!(DOMException.isInstance(ex) && ex.name == "NotFoundError")) {
        console.error(ex);

        // Move the original file to a backup location, ignoring errors.
        try {
          let uniquePath = await IOUtils.createUniqueFile(
            PathUtils.parent(this.path),
            PathUtils.filename(this.path) + ".corrupt",
            0o600
          );
          await IOUtils.move(this.path, uniquePath);
          this._recordTelemetry("invalid_json");
        } catch (e2) {
          console.error(e2);
        }
      }

      if (this._options.backupFile) {
        // Restore the original file from the backup here so fresh writes to empty
        // json files don't happen at any time in the future compromising the backup
        // in the process.
        try {
          await IOUtils.copy(this._options.backupFile, this.path);
        } catch (e) {
          if (!(DOMException.isInstance(e) && e.name == "NotFoundError")) {
            console.error(e);
          }
        }

        try {
          // We still read from the backup file here instead of the original file in case
          // access to the original file is blocked, e.g. by anti-virus software on the
          // user's computer.
          data = await IOUtils.readJSON(
            this._options.backupFile,
            this._options
          );
          // If synchronous loading happened in the meantime, exit now.
          if (this.dataReady) {
            return;
          }
          this._recordTelemetry("used_backup");
        } catch (e3) {
          if (!(DOMException.isInstance(e3) && e3.name == "NotFoundError")) {
            console.error(e3);
          }
        }
      }

      // In some rare cases it's possible for data to have been added to
      // our database between the call to IOUtils.read and when we've been
      // notified that there was a problem with it. In that case, leave the
      // synchronously-added data alone.
      if (this.dataReady) {
        return;
      }
    }

    this._processLoadedData(data);
  },

  /**
   * Loads persistent data from the file to memory, synchronously. An exception
   * can be thrown only if dataPostProcessor exists and fails.
   */
  ensureDataReady() {
    if (this.dataReady) {
      return;
    }

    let data = {};

    try {
      // This reads the file and automatically detects the UTF-8 encoding.
      let inputStream = new FileInputStream(
        new lazy.FileUtils.File(this.path),
        lazy.FileUtils.MODE_RDONLY,
        lazy.FileUtils.PERMS_FILE,
        0
      );
      try {
        let bytes = lazy.NetUtil.readInputStream(
          inputStream,
          inputStream.available()
        );
        data = JSON.parse(lazy.gTextDecoder.decode(bytes));
      } finally {
        inputStream.close();
      }
    } catch (ex) {
      // If an exception occurs because the file does not exist or it cannot be read,
      // we do two things.
      // 1. For consumers of JSONFile.sys.mjs that have configured a `backupTo` path option,
      //    we try to look for and use backed up json files first. If the backup
      //    is also not found or if the backup is unreadable, we then start with an empty file.
      // 2. If a consumer does not configure a `backupTo` path option, we just start
      //    with an empty file.

      // In the event that the file exists, but an exception is thrown because it cannot be read,
      // we store it as a .corrupt file for debugging purposes.
      if (
        !(
          ex instanceof Components.Exception &&
          ex.result == Cr.NS_ERROR_FILE_NOT_FOUND
        )
      ) {
        console.error(ex);
        // Move the original file to a backup location, ignoring errors.
        try {
          let originalFile = new lazy.FileUtils.File(this.path);
          let backupFile = originalFile.clone();
          backupFile.leafName += ".corrupt";
          backupFile.createUnique(
            Ci.nsIFile.NORMAL_FILE_TYPE,
            lazy.FileUtils.PERMS_FILE
          );
          backupFile.remove(false);
          originalFile.moveTo(backupFile.parent, backupFile.leafName);
        } catch (e2) {
          console.error(e2);
        }
      }

      if (this._options.backupFile) {
        // Restore the original file from the backup here so fresh writes to empty
        // json files don't happen at any time in the future compromising the backup
        // in the process.
        try {
          let basename = PathUtils.filename(this.path);
          let backupFile = new lazy.FileUtils.File(this._options.backupFile);
          backupFile.copyTo(null, basename);
        } catch (e) {
          if (e.result != Cr.NS_ERROR_FILE_NOT_FOUND) {
            console.error(e);
          }
        }

        try {
          // We still read from the backup file here instead of the original file in case
          // access to the original file is blocked, e.g. by anti-virus software on the
          // user's computer.
          // This reads the file and automatically detects the UTF-8 encoding.
          let inputStream = new FileInputStream(
            new lazy.FileUtils.File(this._options.backupFile),
            lazy.FileUtils.MODE_RDONLY,
            lazy.FileUtils.PERMS_FILE,
            0
          );
          try {
            let bytes = lazy.NetUtil.readInputStream(
              inputStream,
              inputStream.available()
            );
            data = JSON.parse(lazy.gTextDecoder.decode(bytes));
          } finally {
            inputStream.close();
          }
        } catch (e3) {
          if (e3.result != Cr.NS_ERROR_FILE_NOT_FOUND) {
            console.error(e3);
          }
        }
      }
    }

    this._processLoadedData(data);
  },

  /**
   * Called when the data changed, this triggers asynchronous serialization.
   */
  saveSoon() {
    return this._saver.arm();
  },

  /**
   * Saves persistent data from memory to the file.
   *
   * If an error occurs, the previous file is not deleted.
   *
   * @return {Promise}
   * @resolves When the operation finished successfully.
   * @rejects JavaScript exception.
   */
  async _save() {
    // Create or overwrite the file.
    if (this._beforeSave) {
      await Promise.resolve(this._beforeSave());
    }

    try {
      await IOUtils.writeJSON(
        this.path,
        this._data,
        Object.assign({ tmpPath: this.path + ".tmp" }, this._options)
      );
    } catch (ex) {
      if (typeof this._data.toJSONSafe == "function") {
        // If serialization fails, try fallback safe JSON converter.
        await IOUtils.writeUTF8(
          this.path,
          this._data.toJSONSafe(),
          Object.assign({ tmpPath: this.path + ".tmp" }, this._options)
        );
      } else {
        // Something went wrong with saving that we cannot recover from. If
        // the consumer of JSONFile has supplied a save failure handler, we'll
        // at least let them know.
        try {
          this._saveFailureHandler(ex);
        } catch (saveFailureEx) {
          console.error(
            "Failed to handle ",
            ex,
            " in save failure handler due to ",
            saveFailureEx
          );
        }
      }
    }
  },

  /**
   * Synchronously work on the data just loaded into memory.
   */
  _processLoadedData(data) {
    if (this._finalizePromise) {
      // It's possible for `load` to race with `finalize`. In that case, don't
      // process or set the loaded data.
      return;
    }
    this.data = this._dataPostProcessor ? this._dataPostProcessor(data) : data;
  },

  _recordTelemetry(value) {
    switch (this.sanitizedBasename) {
      case "logins":
        Glean.jsonfile.loadLogins.record({ value });
        break;
      case "autofillprofiles":
        Glean.jsonfile.loadAutofillprofiles.record({ value });
        break;

      default:
      // Avoid recording cases telemetry does not know about.
    }
  },

  /**
   * Finishes persisting data to disk and resets all state for this file.
   *
   * @return {Promise}
   * @resolves When the object is finalized.
   */
  _finalizeInternal() {
    if (this._finalizePromise) {
      // Finalization already in progress; return the pending promise. This is
      // possible if `finalize` is called concurrently with shutdown.
      return this._finalizePromise;
    }
    this._finalizePromise = (async () => {
      await this._saver.finalize();
      this._data = null;
      this.dataReady = false;
    })();
    return this._finalizePromise;
  },

  /**
   * Ensures that all data is persisted to disk, and prevents future calls to
   * `saveSoon`. This is called automatically on shutdown, but can also be
   * called explicitly when the file is no longer needed.
   */
  async finalize() {
    if (this._finalizePromise) {
      throw new Error(`The file ${this.path} has already been finalized`);
    }
    // Wait for finalization before removing the shutdown blocker.
    await this._finalizeInternal();
    this._finalizeAt.removeBlocker(this._finalizeInternalBound);
  },
};
