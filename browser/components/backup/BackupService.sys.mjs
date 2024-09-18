/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import * as DefaultBackupResources from "resource:///modules/backup/BackupResources.sys.mjs";
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { BackupResource } from "resource:///modules/backup/BackupResource.sys.mjs";
import {
  MeasurementUtils,
  BYTES_IN_KILOBYTE,
  BYTES_IN_MEGABYTE,
  BYTES_IN_MEBIBYTE,
} from "resource:///modules/backup/MeasurementUtils.sys.mjs";

import { ERRORS } from "chrome://browser/content/backup/backup-constants.mjs";
import { BackupError } from "resource:///modules/backup/BackupError.mjs";

const BACKUP_DIR_PREF_NAME = "browser.backup.location";
const SCHEDULED_BACKUPS_ENABLED_PREF_NAME = "browser.backup.scheduled.enabled";
const IDLE_THRESHOLD_SECONDS_PREF_NAME =
  "browser.backup.scheduled.idle-threshold-seconds";
const MINIMUM_TIME_BETWEEN_BACKUPS_SECONDS_PREF_NAME =
  "browser.backup.scheduled.minimum-time-between-backups-seconds";
const LAST_BACKUP_TIMESTAMP_PREF_NAME =
  "browser.backup.scheduled.last-backup-timestamp";
const LAST_BACKUP_FILE_NAME_PREF_NAME =
  "browser.backup.scheduled.last-backup-file";

const SCHEMAS = Object.freeze({
  BACKUP_MANIFEST: 1,
  ARCHIVE_JSON_BLOCK: 2,
});

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "logConsole", function () {
  return console.createInstance({
    prefix: "BackupService",
    maxLogLevel: Services.prefs.getBoolPref("browser.backup.log", false)
      ? "Debug"
      : "Warn",
  });
});

ChromeUtils.defineLazyGetter(lazy, "fxAccounts", () => {
  return ChromeUtils.importESModule(
    "resource://gre/modules/FxAccounts.sys.mjs"
  ).getFxAccountsSingleton();
});

ChromeUtils.defineESModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  ArchiveDecryptor: "resource:///modules/backup/ArchiveEncryption.sys.mjs",
  ArchiveEncryptionState:
    "resource:///modules/backup/ArchiveEncryptionState.sys.mjs",
  ArchiveUtils: "resource:///modules/backup/ArchiveUtils.sys.mjs",
  BasePromiseWorker: "resource://gre/modules/PromiseWorker.sys.mjs",
  ClientID: "resource://gre/modules/ClientID.sys.mjs",
  DeferredTask: "resource://gre/modules/DeferredTask.sys.mjs",
  DownloadPaths: "resource://gre/modules/DownloadPaths.sys.mjs",
  FileUtils: "resource://gre/modules/FileUtils.sys.mjs",
  JsonSchema: "resource://gre/modules/JsonSchema.sys.mjs",
  NetUtil: "resource://gre/modules/NetUtil.sys.mjs",
  UIState: "resource://services-sync/UIState.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "ZipWriter", () =>
  Components.Constructor("@mozilla.org/zipwriter;1", "nsIZipWriter", "open")
);
ChromeUtils.defineLazyGetter(lazy, "ZipReader", () =>
  Components.Constructor(
    "@mozilla.org/libjar/zip-reader;1",
    "nsIZipReader",
    "open"
  )
);
ChromeUtils.defineLazyGetter(lazy, "nsLocalFile", () =>
  Components.Constructor("@mozilla.org/file/local;1", "nsIFile", "initWithPath")
);

ChromeUtils.defineLazyGetter(lazy, "BinaryInputStream", () =>
  Components.Constructor(
    "@mozilla.org/binaryinputstream;1",
    "nsIBinaryInputStream",
    "setInputStream"
  )
);

ChromeUtils.defineLazyGetter(lazy, "gFluentStrings", function () {
  return new Localization(
    ["branding/brand.ftl", "preview/backupSettings.ftl"],
    true
  );
});

ChromeUtils.defineLazyGetter(lazy, "gDOMLocalization", function () {
  return new DOMLocalization([
    "branding/brand.ftl",
    "preview/backupSettings.ftl",
  ]);
});

ChromeUtils.defineLazyGetter(lazy, "defaultParentDirPath", function () {
  return Services.dirsvc.get("Docs", Ci.nsIFile).path;
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "scheduledBackupsPref",
  SCHEDULED_BACKUPS_ENABLED_PREF_NAME,
  false,
  function onUpdateScheduledBackups(_pref, _prevVal, newVal) {
    let bs = BackupService.get();
    if (bs) {
      bs.onUpdateScheduledBackups(newVal);
    }
  }
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "backupDirPref",
  BACKUP_DIR_PREF_NAME,
  /**
   * To avoid disk access upon startup, do not set DEFAULT_PARENT_DIR_PATH
   * as a fallback value here. Let registered widgets prompt BackupService
   * to update the parentDirPath.
   *
   * @see BackupService.state
   * @see DEFAULT_PARENT_DIR_PATH
   * @see setParentDirPath
   */
  null,
  async function onUpdateLocationDirPath(_pref, _prevVal, newVal) {
    let bs = BackupService.get();
    if (bs) {
      await bs.onUpdateLocationDirPath(newVal);
    }
  }
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "minimumTimeBetweenBackupsSeconds",
  MINIMUM_TIME_BETWEEN_BACKUPS_SECONDS_PREF_NAME,
  3600 /* 1 hour */
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "idleService",
  "@mozilla.org/widget/useridleservice;1",
  "nsIUserIdleService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "nativeOSKeyStore",
  "@mozilla.org/security/oskeystore;1",
  Ci.nsIOSKeyStore
);

/**
 * A class that wraps a multipart/mixed stream converter instance, and streams
 * in the binary part of a single-file archive (which should be at the second
 * index of the attachments) as a ReadableStream.
 *
 * The bytes that are read in are text decoded, but are not guaranteed to
 * represent a "full chunk" of base64 data. Consumers should ensure to buffer
 * the strings emitted by this stream, and to search for `\n` characters, which
 * indicate the end of a (potentially encrypted and) base64 encoded block.
 */
class BinaryReadableStream {
  #channel = null;

  /**
   * Constructs a BinaryReadableStream.
   *
   * @param {nsIChannel} channel
   *   The channel through which to begin the flow of bytes from the
   *   inputStream
   */
  constructor(channel) {
    this.#channel = channel;
  }

  /**
   * Implements `start` from the `underlyingSource` of a ReadableStream
   *
   * @param {ReadableStreamDefaultController} controller
   *   The controller for the ReadableStream to feed strings into.
   */
  start(controller) {
    let streamConv = Cc["@mozilla.org/streamConverters;1"].getService(
      Ci.nsIStreamConverterService
    );

    let textDecoder = new TextDecoder();

    // The attachment index that should contain the binary data.
    const EXPECTED_CONTENT_TYPE = "application/octet-stream";

    // This is fairly clumsy, but by using an object nsIStreamListener like
    // this, I can keep from stashing the `controller` somewhere, as it's
    // available in the closure.
    let multipartListenerForBinary = {
      /**
       * True once we've found an attachment matching our EXPECTED_CONTENT_TYPE.
       * Once this is true, bytes flowing into onDataAvailable will be
       * enqueued through the controller.
       *
       * @type {boolean}
       */
      _enabled: false,

      /**
       * True once onStopRequest has been called once the listener is enabled.
       * After this, the listener will not attempt to read any data passed
       * to it through onDataAvailable.
       *
       * @type {boolean}
       */
      _done: false,

      QueryInterface: ChromeUtils.generateQI([
        "nsIStreamListener",
        "nsIRequestObserver",
        "nsIMultiPartChannelListener",
      ]),

      /**
       * Called when we begin to load an attachment from the MIME message.
       *
       * @param {nsIRequest} request
       *   The request corresponding to the source of the data.
       */
      onStartRequest(request) {
        if (!(request instanceof Ci.nsIChannel)) {
          throw Components.Exception(
            "onStartRequest expected an nsIChannel request",
            Cr.NS_ERROR_UNEXPECTED
          );
        }
        this._enabled = request.contentType == EXPECTED_CONTENT_TYPE;
      },

      /**
       * Called when data is flowing in for an attachment.
       *
       * @param {nsIRequest} request
       *   The request corresponding to the source of the data.
       * @param {nsIInputStream} stream
       *   The input stream containing the data chunk.
       * @param {number} offset
       *   The number of bytes that were sent in previous onDataAvailable calls
       *   for this request. In other words, the sum of all previous count
       *   parameters.
       * @param {number} count
       *   The number of bytes available in the stream
       */
      onDataAvailable(request, stream, offset, count) {
        if (!this._enabled) {
          // We don't care about this data, just move on.
          return;
        }

        let binStream = new lazy.BinaryInputStream(stream);
        let bytes = new Uint8Array(count);
        binStream.readArrayBuffer(count, bytes.buffer);
        let string = textDecoder.decode(bytes);
        controller.enqueue(string);
      },

      /**
       * Called when the load of an attachment finishes.
       */
      onStopRequest() {
        if (this._enabled && !this._done) {
          this._enabled = false;
          this._done = true;

          controller.close();

          // No need to load anything else - abort reading in more
          // attachments.
          throw Components.Exception(
            "Got binary block - cancelling loading the multipart stream.",
            Cr.NS_BINDING_ABORTED
          );
        }
      },

      onAfterLastPart() {
        if (!this._done) {
          // We finished reading the parts before we found the binary block,
          // so the binary block is missing.
          controller.error(
            new BackupError(
              "Could not find binary block.",
              ERRORS.CORRUPTED_ARCHIVE
            )
          );
        }
      },
    };

    let conv = streamConv.asyncConvertData(
      "multipart/mixed",
      "*/*",
      multipartListenerForBinary,
      null
    );

    this.#channel.asyncOpen(conv);
  }
}

/**
 * A TransformStream class that takes in chunks of base64 encoded data,
 * decodes (and eventually, decrypts) them before passing the resulting
 * bytes along to the next step in the pipe.
 *
 * The BinaryReadableStream feeds strings into this TransformStream, but the
 * buffering of these streams means that we cannot be certain that the string
 * that was passed is the entirety of a base64 encoded block. ArchiveWorker
 * puts every block on its own line, meaning that we must simply look for
 * newlines to indicate when a break between full blocks is, and buffer chunks
 * until we see those breaks - only decoding once we have a full block.
 */
export class DecoderDecryptorTransformer {
  #buffer = "";
  #decryptor = null;

  /**
   * Constructs the DecoderDecryptorTransformer.
   *
   * @param {ArchiveDecryptor|null} decryptor
   *   An initialized ArchiveDecryptor, if this stream of bytes is presumed to
   *   be encrypted.
   */
  constructor(decryptor) {
    this.#decryptor = decryptor;
  }

  /**
   * Consumes a single chunk of a base64 encoded string sent by
   * BinaryReadableStream.
   *
   * @param {string} chunkPart
   *   A part of a chunk of a base64 encoded string sent by
   *   BinaryReadableStream.
   * @param {TransformStreamDefaultController} controller
   *   The controller to send decoded bytes to.
   * @returns {Promise<undefined>}
   */
  async transform(chunkPart, controller) {
    // A small optimization, but considering the size of these strings, it's
    // likely worth it.
    if (this.#buffer) {
      this.#buffer += chunkPart;
    } else {
      this.#buffer = chunkPart;
    }

    // If the compressed archive was large enough, then it got split up over
    // several chunks. In that case, each chunk is separated by a newline. We
    // also filter out any extraneous newlines that might have been included
    // at the end.
    let chunks = this.#buffer.split("\n").filter(chunk => chunk != "");

    this.#buffer = chunks.pop();
    // If there were any remaining parts that we split out from the buffer,
    // they must constitute full blocks that we can decode.
    for (let chunk of chunks) {
      await this.#processChunk(controller, chunk);
    }
  }

  /**
   * Called once BinaryReadableStream signals that it has sent all of its
   * strings, in which case we know that whatever is in the buffer should be
   * a valid block.
   *
   * @param {TransformStreamDefaultController} controller
   *   The controller to send decoded bytes to.
   * @returns {Promise<undefined>}
   */
  async flush(controller) {
    await this.#processChunk(controller, this.#buffer, true);
    this.#buffer = "";
  }

  /**
   * Decodes (and potentially decrypts) a valid base64 encoded chunk into a
   * Uint8Array and sends it to the next step in the pipe.
   *
   * @param {TransformStreamDefaultController} controller
   *   The controller to send decoded bytes to.
   * @param {string} chunk
   *   The base64 encoded string to decode and potentially decrypt.
   * @param {boolean} [isLastChunk=false]
   *   True if this is the last chunk to be processed.
   * @returns {Promise<undefined>}
   */
  async #processChunk(controller, chunk, isLastChunk = false) {
    try {
      let bytes = lazy.ArchiveUtils.stringToArray(chunk);

      if (this.#decryptor) {
        let plaintextBytes = await this.#decryptor.decrypt(bytes, isLastChunk);
        controller.enqueue(plaintextBytes);
      } else {
        controller.enqueue(bytes);
      }
    } catch (e) {
      // Something went wrong base64 decoding or decrypting. Tell the controller
      // that we're done, so that it can destroy anything that was decoded /
      // decrypted already.
      controller.error("Corrupted archive.");
    }
  }
}

/**
 * A class that lets us construct a WritableStream that writes bytes to a file
 * on disk somewhere.
 */
export class FileWriterStream {
  /**
   * @type {string}
   */
  #destPath = null;

  /**
   * @type {nsIOutputStream}
   */
  #outStream = null;

  /**
   * @type {nsIBinaryOutputStream}
   */
  #binStream = null;

  /**
   * @type {ArchiveDecryptor}
   */
  #decryptor = null;

  /**
   * Constructor for FileWriterStream.
   *
   * @param {string} destPath
   *   The path to write the incoming bytes to.
   * @param {ArchiveDecryptor|null} decryptor
   *   An initialized ArchiveDecryptor, if this stream of bytes is presumed to
   *   be encrypted.
   */
  constructor(destPath, decryptor) {
    this.#destPath = destPath;
    this.#decryptor = decryptor;
  }

  /**
   * Called once the first set of bytes comes in from the
   * DecoderDecryptorTransformer. This creates the file, and sets up the
   * underlying nsIOutputStream mechanisms to let us write bytes to the file.
   */
  async start() {
    let extractionDestFile = await IOUtils.getFile(this.#destPath);
    this.#outStream =
      lazy.FileUtils.openSafeFileOutputStream(extractionDestFile);
    this.#binStream = Cc["@mozilla.org/binaryoutputstream;1"].createInstance(
      Ci.nsIBinaryOutputStream
    );
    this.#binStream.setOutputStream(this.#outStream);
  }

  /**
   * Writes bytes to the destination on the file system.
   *
   * @param {Uint8Array} chunk
   *   The bytes to stream to the destination file.
   */
  write(chunk) {
    this.#binStream.writeByteArray(chunk);
  }

  /**
   * Called once the stream of bytes finishes flowing in and closes the stream.
   *
   * @param {WritableStreamDefaultController} controller
   *   The controller for the WritableStream.
   */
  close(controller) {
    lazy.FileUtils.closeSafeFileOutputStream(this.#outStream);
    if (this.#decryptor && !this.#decryptor.isDone()) {
      lazy.logConsole.error(
        "Decryptor was not done when the stream was closed."
      );
      controller.error("Corrupted archive.");
    }
  }

  /**
   * Called if something went wrong while decoding / decrypting the stream of
   * bytes. This destroys any bytes that may have been decoded / decrypted
   * prior to the error.
   *
   * @param {string} reason
   *   The reported reason for aborting the decoding / decrpytion.
   */
  async abort(reason) {
    lazy.logConsole.error(`Writing to ${this.#destPath} failed: `, reason);
    lazy.FileUtils.closeSafeFileOutputStream(this.#outStream);
    await IOUtils.remove(this.#destPath, {
      ignoreAbsent: true,
      retryReadonly: true,
    });
  }
}

/**
 * The BackupService class orchestrates the scheduling and creation of profile
 * backups. It also does most of the heavy lifting for the restoration of a
 * profile backup.
 */
export class BackupService extends EventTarget {
  /**
   * The BackupService singleton instance.
   *
   * @static
   * @type {BackupService|null}
   */
  static #instance = null;

  /**
   * Map of instantiated BackupResource classes.
   *
   * @type {Map<string, BackupResource>}
   */
  #resources = new Map();

  /**
   * The name of the backup folder. Should be localized.
   *
   * @see BACKUP_DIR_NAME
   */
  static #backupFolderName = null;

  /**
   * The name of the backup archive file. Should be localized.
   *
   * @see BACKUP_FILE_NAME
   */
  static #backupFileName = null;

  /**
   * Set to true if a backup is currently in progress. Causes stateUpdate()
   * to be called.
   *
   * @see BackupService.stateUpdate()
   * @param {boolean} val
   *   True if a backup is in progress.
   */
  set #backupInProgress(val) {
    if (this.#_state.backupInProgress != val) {
      this.#_state.backupInProgress = val;
      this.stateUpdate();
    }
  }

  /**
   * True if a backup is currently in progress.
   *
   * @type {boolean}
   */
  get #backupInProgress() {
    return this.#_state.backupInProgress;
  }

  /**
   * Dispatches an event to let listeners know that the BackupService state
   * object has been updated.
   */
  stateUpdate() {
    this.dispatchEvent(new CustomEvent("BackupService:StateUpdate"));
  }

  /**
   * True if a recovery is currently in progress.
   *
   * @type {boolean}
   */
  #recoveryInProgress = false;

  /**
   * An object holding the current state of the BackupService instance, for
   * the purposes of representing it in the user interface. Ideally, this would
   * be named #state instead of #_state, but sphinx-js seems to be fairly
   * unhappy with that coupled with the ``state`` getter.
   *
   * @type {object}
   */
  #_state = {
    backupDirPath: lazy.backupDirPref,
    defaultParent: {},
    backupFileToRestore: null,
    backupFileInfo: null,
    backupInProgress: false,
    scheduledBackupsEnabled: lazy.scheduledBackupsPref,
    encryptionEnabled: false,
    /** @type {number?} Number of seconds since UNIX epoch */
    lastBackupDate: null,
    lastBackupFileName: "",
    supportBaseLink: Services.urlFormatter.formatURLPref("app.support.baseURL"),
  };

  /**
   * A Promise that will resolve once the postRecovery steps are done. It will
   * also resolve if postRecovery steps didn't need to run.
   *
   * @see BackupService.checkForPostRecovery()
   * @type {Promise<undefined>}
   */
  #postRecoveryPromise;

  /**
   * The resolving function for #postRecoveryPromise, which should be called
   * by checkForPostRecovery() before exiting.
   *
   * @type {Function}
   */
  #postRecoveryResolver;

  /**
   * The currently used ArchiveEncryptionState. Callers should use
   * loadEncryptionState() instead, to ensure that any pre-serialized
   * encryption state has been read in and deserialized.
   *
   * This member can be in 3 states:
   *
   * 1. undefined - no attempt has been made to load encryption state from
   *    disk yet.
   * 2. null - encryption is not enabled.
   * 3. ArchiveEncryptionState - encryption is enabled.
   *
   * @see BackupService.loadEncryptionState()
   * @type {ArchiveEncryptionState|null|undefined}
   */
  #encState = undefined;

  /**
   * The PlacesObserver instance used to monitor the Places database for
   * history and bookmark removals to determine if backups should be
   * regenerated.
   *
   * @type {PlacesObserver|null}
   */
  #placesObserver = null;

  /**
   * The AbortController used to abort any queued requests to create or delete
   * backups that might be waiting on the WRITE_BACKUP_LOCK_NAME lock.
   *
   * @type {AbortController}
   */
  #backupWriteAbortController = null;

  /**
   * A DeferredTask that will cause the last known backup to be deleted, and
   * a new backup to be created.
   *
   * See BackupService.#debounceRegeneration()
   *
   * @type {DeferredTask}
   */
  #regenerationDebouncer = null;

  /**
   * The path of the default parent directory for saving backups.
   * The current default is the Documents directory.
   *
   * @returns {string} The path of the default parent directory
   */
  static get DEFAULT_PARENT_DIR_PATH() {
    return lazy.defaultParentDirPath;
  }

  /**
   * The localized name for the user's backup folder.
   *
   * @returns {string} The localized backup folder name
   */
  static get BACKUP_DIR_NAME() {
    if (!BackupService.#backupFolderName) {
      BackupService.#backupFolderName = lazy.DownloadPaths.sanitize(
        lazy.gFluentStrings.formatValueSync("backup-folder-name")
      );
    }
    return BackupService.#backupFolderName;
  }

  /**
   * The localized name for the user's backup archive file. This will have
   * `.html` appended to it before writing the archive file.
   *
   * @returns {string} The localized backup file name
   */
  static get BACKUP_FILE_NAME() {
    if (!BackupService.#backupFileName) {
      BackupService.#backupFileName = lazy.DownloadPaths.sanitize(
        lazy.gFluentStrings.formatValueSync("backup-file-name")
      );
    }
    return BackupService.#backupFileName;
  }

  /**
   * The name of the folder within the profile folder where this service reads
   * and writes state to.
   *
   * @type {string}
   */
  static get PROFILE_FOLDER_NAME() {
    return "backups";
  }

  /**
   * The name of the folder within the PROFILE_FOLDER_NAME where the staging
   * folder / prior backups will be stored.
   *
   * @type {string}
   */
  static get SNAPSHOTS_FOLDER_NAME() {
    return "snapshots";
  }

  /**
   * The name of the backup manifest file.
   *
   * @type {string}
   */
  static get MANIFEST_FILE_NAME() {
    return "backup-manifest.json";
  }

  /**
   * A promise that resolves to the schema for the backup manifest that this
   * BackupService uses when creating a backup. This should be accessed via
   * the `MANIFEST_SCHEMA` static getter.
   *
   * @type {Promise<object>}
   */
  static #manifestSchemaPromise = null;

  /**
   * The current schema version of the backup manifest that this BackupService
   * uses when creating a backup.
   *
   * @type {Promise<object>}
   */
  static get MANIFEST_SCHEMA() {
    if (!BackupService.#manifestSchemaPromise) {
      BackupService.#manifestSchemaPromise = BackupService.getSchemaForVersion(
        SCHEMAS.BACKUP_MANIFEST,
        lazy.ArchiveUtils.SCHEMA_VERSION
      );
    }

    return BackupService.#manifestSchemaPromise;
  }

  /**
   * The name of the post recovery file written into the newly created profile
   * directory just after a profile is recovered from a backup.
   *
   * @type {string}
   */
  static get POST_RECOVERY_FILE_NAME() {
    return "post-recovery.json";
  }

  /**
   * The name of the serialized ArchiveEncryptionState that is written to disk
   * if encryption is enabled.
   *
   * @type {string}
   */
  static get ARCHIVE_ENCRYPTION_STATE_FILE() {
    return "enc-state.json";
  }

  /**
   * Returns the SCHEMAS constants, which is a key/value store of constants.
   *
   * @type {object}
   */
  static get SCHEMAS() {
    return SCHEMAS;
  }

  /**
   * Returns the filename used for the intermediary compressed ZIP file that
   * is extracted from archives during recovery.
   *
   * @type {string}
   */
  static get RECOVERY_ZIP_FILE_NAME() {
    return "recovery.zip";
  }

  /**
   * Returns the schema for the schemaType for a given version.
   *
   * @param {number} schemaType
   *   One of the constants from SCHEMAS.
   * @param {number} version
   *   The version of the schema to return.
   * @returns {Promise<object>}
   */
  static async getSchemaForVersion(schemaType, version) {
    let schemaURL;

    if (schemaType == SCHEMAS.BACKUP_MANIFEST) {
      schemaURL = `chrome://browser/content/backup/BackupManifest.${version}.schema.json`;
    } else if (schemaType == SCHEMAS.ARCHIVE_JSON_BLOCK) {
      schemaURL = `chrome://browser/content/backup/ArchiveJSONBlock.${version}.schema.json`;
    } else {
      throw new BackupError(
        `Did not recognize SCHEMAS constant: ${schemaType}`,
        ERRORS.UNKNOWN
      );
    }

    let response = await fetch(schemaURL);
    return response.json();
  }

  /**
   * The level of Zip compression to use on the zipped staging folder.
   *
   * @type {number}
   */
  static get COMPRESSION_LEVEL() {
    return Ci.nsIZipWriter.COMPRESSION_BEST;
  }

  /**
   * Returns the chrome:// URI string for the template that should be used to
   * construct the single-file archive.
   *
   * @type {string}
   */
  static get ARCHIVE_TEMPLATE() {
    return "chrome://browser/content/backup/archive.template.html";
  }

  /**
   * The native OSKeyStore label used for the temporary recovery store. The
   * temporary recovery store is initialized with the original OSKeyStore
   * secret that was included in an encrypted backup, and then used by any
   * BackupResource's that need to decrypt / re-encrypt OSKeyStore secrets for
   * the current device.
   *
   * @type {string}
   */
  static get RECOVERY_OSKEYSTORE_LABEL() {
    return AppConstants.MOZ_APP_BASENAME + " Backup Recovery Storage";
  }

  /**
   * The name of the exclusive Web Lock that will be requested and held when
   * creating or deleting a backup.
   *
   * @type {string}
   */
  static get WRITE_BACKUP_LOCK_NAME() {
    return "write-backup";
  }

  /**
   * The amount of time (in milliseconds) to wait for our backup regeneration
   * debouncer to kick off a regeneration.
   *
   * @type {number}
   */
  static get REGENERATION_DEBOUNCE_RATE_MS() {
    return 10000;
  }

  /**
   * Returns a reference to a BackupService singleton. If this is the first time
   * that this getter is accessed, this causes the BackupService singleton to be
   * be instantiated.
   *
   * @static
   * @type {BackupService}
   */
  static init() {
    if (this.#instance) {
      return this.#instance;
    }
    this.#instance = new BackupService(DefaultBackupResources);

    this.#instance.checkForPostRecovery().then(() => {
      this.#instance.takeMeasurements();
    });

    this.#instance.initBackupScheduler();
    return this.#instance;
  }

  /**
   * Returns a reference to the BackupService singleton. If the singleton has
   * not been initialized, an error is thrown.
   *
   * @static
   * @returns {BackupService}
   */
  static get() {
    if (!this.#instance) {
      throw new BackupError(
        "BackupService not initialized",
        ERRORS.UNINITIALIZED
      );
    }
    return this.#instance;
  }

  /**
   * Create a BackupService instance.
   *
   * @param {object} [backupResources=DefaultBackupResources]
   *   Object containing BackupResource classes to associate with this service.
   */
  constructor(backupResources = DefaultBackupResources) {
    super();
    lazy.logConsole.debug("Instantiated");

    for (const resourceName in backupResources) {
      let resource = backupResources[resourceName];
      this.#resources.set(resource.key, resource);
    }

    let { promise, resolve } = Promise.withResolvers();
    this.#postRecoveryPromise = promise;
    this.#postRecoveryResolver = resolve;
    this.#backupWriteAbortController = new AbortController();
    this.#regenerationDebouncer = new lazy.DeferredTask(async () => {
      if (!this.#backupWriteAbortController.signal.aborted) {
        await this.deleteLastBackup();
        await this.createBackupOnIdleDispatch();
      }
    }, BackupService.REGENERATION_DEBOUNCE_RATE_MS);
  }

  /**
   * Returns a reference to a Promise that will resolve with undefined once
   * postRecovery steps have had a chance to run. This will also be resolved
   * with undefined if no postRecovery steps needed to be run.
   *
   * @see BackupService.checkForPostRecovery()
   * @returns {Promise<undefined>}
   */
  get postRecoveryComplete() {
    return this.#postRecoveryPromise;
  }

  /**
   * Returns a state object describing the state of the BackupService for the
   * purposes of representing it in the user interface. The returned state
   * object is immutable.
   *
   * @type {object}
   */
  get state() {
    if (!Object.keys(this.#_state.defaultParent).length) {
      let defaultPath = BackupService.DEFAULT_PARENT_DIR_PATH;
      this.#_state.defaultParent = {
        path: defaultPath,
        fileName: PathUtils.filename(defaultPath),
        iconURL: this.getIconFromFilePath(defaultPath),
      };
    }

    return Object.freeze(structuredClone(this.#_state));
  }

  /**
   * Attempts to find the right folder to write the single-file archive to, and
   * if it does not exist, to create it.
   *
   * If the configured destination's parent folder does not exist and cannot
   * be recreated, we will fall back to the `defaultParentDirPath`. If
   * `defaultParentDirPath` happens to not exist or cannot be created, we will
   * fall back to the home directory. If _that_ folder does not exist and cannot
   * be recreated, this method will reject.
   *
   * @param {string} configuredDestFolderPath
   *   The currently configured destination folder for the archive.
   * @returns {Promise<string, Error>}
   */
  async resolveArchiveDestFolderPath(configuredDestFolderPath) {
    lazy.logConsole.log(
      "Resolving configured archive destination folder: ",
      configuredDestFolderPath
    );

    // Try to create the configured folder ancestry. If that fails, we clear
    // configuredDestFolderPath so that we can try the fallback paths, as
    // if the folder was never set.
    try {
      await IOUtils.makeDirectory(configuredDestFolderPath, {
        createAncestors: true,
        ignoreExisting: true,
      });
      return configuredDestFolderPath;
    } catch (e) {
      lazy.logConsole.warn("Could not create configured destination path: ", e);
    }

    lazy.logConsole.warn(
      "The destination directory was invalid. Attempting to fall back to " +
        "default parent folder: ",
      BackupService.DEFAULT_PARENT_DIR_PATH
    );
    let fallbackFolderPath = PathUtils.join(
      BackupService.DEFAULT_PARENT_DIR_PATH,
      BackupService.BACKUP_DIR_NAME
    );
    try {
      await IOUtils.makeDirectory(fallbackFolderPath, {
        createAncestors: true,
        ignoreExisting: true,
      });
      return fallbackFolderPath;
    } catch (e) {
      lazy.logConsole.warn("Could not create fallback destination path: ", e);
    }

    let homeDirPath = PathUtils.join(
      Services.dirsvc.get("Home", Ci.nsIFile).path,
      BackupService.BACKUP_DIR_NAME
    );
    lazy.logConsole.warn(
      "The destination directory was invalid. Attempting to fall back to " +
        "Home folder: ",
      homeDirPath
    );
    try {
      await IOUtils.makeDirectory(homeDirPath, {
        createAncestors: true,
        ignoreExisting: true,
      });
      return homeDirPath;
    } catch (e) {
      lazy.logConsole.warn("Could not create Home destination path: ", e);
      throw new Error(
        "Could not resolve to a writable destination folder path.",
        { cause: ERRORS.FILE_SYSTEM_ERROR }
      );
    }
  }

  /**
   * Computes the appropriate link to place in the single-file archive for
   * downloading a version of this application for the same update channel.
   *
   * When bug 1905909 lands, we'll first check to see if there are download
   * links available in Remote Settings.
   *
   * If there aren't any, we will fallback by looking for preference values at
   * browser.backup.template.fallback-download.${updateChannel}.
   *
   * If no such preference exists, a final "ultimate" fallback download link is
   * chosen for the release channel.
   *
   * @param {string} updateChannel
   *  The current update channel for the application, as provided by
   *  AppConstants.MOZ_UPDATE_CHANNEL.
   * @returns {Promise<string>}
   */
  async resolveDownloadLink(updateChannel) {
    // If all else fails, this is the download link we'll put into the rendered
    // template.
    const ULTIMATE_FALLBACK_DOWNLOAD_URL =
      "https://www.mozilla.org/firefox/download/thanks/?s=direct&utm_medium=firefox-desktop&utm_source=backup&utm_campaign=firefox-backup-2024&utm_content=control";
    const FALLBACK_DOWNLOAD_URL = Services.prefs.getStringPref(
      `browser.backup.template.fallback-download.${updateChannel}`,
      ULTIMATE_FALLBACK_DOWNLOAD_URL
    );

    // Bug 1905909: Once we set up the download links in RemoteSettings, we can
    // query for them here.

    return FALLBACK_DOWNLOAD_URL;
  }

  /**
   * @typedef {object} CreateBackupResult
   * @property {object} manifest
   *   The backup manifest data of the created backup. See BackupManifest
   *   schema for specific details.
   * @property {string} archivePath
   *   The path to the single file archive that was created.
   */

  /**
   * Create a backup of the user's profile.
   *
   * @param {object} [options]
   *   Options for the backup.
   * @param {string} [options.profilePath=PathUtils.profileDir]
   *   The path to the profile to backup. By default, this is the current
   *   profile.
   * @returns {Promise<CreateBackupResult|null>}
   *   A promise that resolves to information about the backup that was
   *   created, or null if the backup failed.
   */
  async createBackup({ profilePath = PathUtils.profileDir } = {}) {
    // createBackup does not allow re-entry or concurrent backups.
    if (this.#backupInProgress) {
      lazy.logConsole.warn("Backup attempt already in progress");
      return null;
    }

    return locks.request(
      BackupService.WRITE_BACKUP_LOCK_NAME,
      { signal: this.#backupWriteAbortController.signal },
      async () => {
        this.#backupInProgress = true;
        const backupTimer = Glean.browserBackup.totalBackupTime.start();

        try {
          lazy.logConsole.debug(
            `Creating backup for profile at ${profilePath}`
          );

          let archiveDestFolderPath = await this.resolveArchiveDestFolderPath(
            lazy.backupDirPref
          );
          lazy.logConsole.debug(
            `Destination for archive: ${archiveDestFolderPath}`
          );

          let manifest = await this.#createBackupManifest();

          // First, check to see if a `backups` directory already exists in the
          // profile.
          let backupDirPath = PathUtils.join(
            profilePath,
            BackupService.PROFILE_FOLDER_NAME,
            BackupService.SNAPSHOTS_FOLDER_NAME
          );
          lazy.logConsole.debug("Creating backups folder");

          // ignoreExisting: true is the default, but we're being explicit that it's
          // okay if this folder already exists.
          await IOUtils.makeDirectory(backupDirPath, {
            ignoreExisting: true,
            createAncestors: true,
          });

          let stagingPath = await this.#prepareStagingFolder(backupDirPath);

          // Sort resources be priority.
          let sortedResources = Array.from(this.#resources.values()).sort(
            (a, b) => {
              return b.priority - a.priority;
            }
          );

          let encState = await this.loadEncryptionState(profilePath);
          let encryptionEnabled = !!encState;
          lazy.logConsole.debug("Encryption enabled: ", encryptionEnabled);

          // Perform the backup for each resource.
          for (let resourceClass of sortedResources) {
            try {
              lazy.logConsole.debug(
                `Backing up resource with key ${resourceClass.key}. ` +
                  `Requires encryption: ${resourceClass.requiresEncryption}`
              );

              if (resourceClass.requiresEncryption && !encryptionEnabled) {
                lazy.logConsole.debug(
                  "Encryption is not currently enabled. Skipping."
                );
                continue;
              }

              let resourcePath = PathUtils.join(stagingPath, resourceClass.key);
              await IOUtils.makeDirectory(resourcePath);

              // `backup` on each BackupResource should return us a ManifestEntry
              // that we eventually write to a JSON manifest file, but for now,
              // we're just going to log it.
              let manifestEntry = await new resourceClass().backup(
                resourcePath,
                profilePath,
                encryptionEnabled
              );

              if (manifestEntry === undefined) {
                lazy.logConsole.error(
                  `Backup of resource with key ${resourceClass.key} returned undefined
                as its ManifestEntry instead of null or an object`
                );
              } else {
                lazy.logConsole.debug(
                  `Backup of resource with key ${resourceClass.key} completed`,
                  manifestEntry
                );
                manifest.resources[resourceClass.key] = manifestEntry;
              }
            } catch (e) {
              lazy.logConsole.error(
                `Failed to backup resource: ${resourceClass.key}`,
                e
              );
            }
          }

          // Ensure that the manifest abides by the current schema, and log
          // an error if somehow it doesn't. We'll want to collect telemetry for
          // this case to make sure it's not happening in the wild. We debated
          // throwing an exception here too, but that's not meaningfully better
          // than creating a backup that's not schema-compliant. At least in this
          // case, a user so-inclined could theoretically repair the manifest
          // to make it valid.
          let manifestSchema = await BackupService.MANIFEST_SCHEMA;
          let schemaValidationResult = lazy.JsonSchema.validate(
            manifest,
            manifestSchema
          );
          if (!schemaValidationResult.valid) {
            lazy.logConsole.error(
              "Backup manifest does not conform to schema:",
              manifest,
              manifestSchema,
              schemaValidationResult
            );
            // TODO: Collect telemetry for this case. (bug 1891817)
          }

          // Write the manifest to the staging folder.
          let manifestPath = PathUtils.join(
            stagingPath,
            BackupService.MANIFEST_FILE_NAME
          );
          await IOUtils.writeJSON(manifestPath, manifest);

          let renamedStagingPath = await this.#finalizeStagingFolder(
            stagingPath
          );
          lazy.logConsole.log(
            "Wrote backup to staging directory at ",
            renamedStagingPath
          );

          // Record the total size of the backup staging directory
          let totalSizeKilobytes = await BackupResource.getDirectorySize(
            renamedStagingPath
          );
          let totalSizeBytesNearestMebibyte = MeasurementUtils.fuzzByteSize(
            totalSizeKilobytes * BYTES_IN_KILOBYTE,
            1 * BYTES_IN_MEBIBYTE
          );
          lazy.logConsole.debug(
            "total staging directory size in bytes: " +
              totalSizeBytesNearestMebibyte
          );

          Glean.browserBackup.totalBackupSize.accumulate(
            totalSizeBytesNearestMebibyte / BYTES_IN_MEBIBYTE
          );

          let compressedStagingPath = await this.#compressStagingFolder(
            renamedStagingPath,
            backupDirPath
          ).finally(async () => {
            await IOUtils.remove(renamedStagingPath, { recursive: true });
          });

          // Now create the single-file archive. For now, we'll stash this in the
          // backups folder while it gets written. Once that's done, we'll attempt
          // to move it to the user's configured backup path.
          let archiveTmpPath = PathUtils.join(backupDirPath, "archive.html");
          lazy.logConsole.log(
            "Exporting single-file archive to ",
            archiveTmpPath
          );
          await this.createArchive(
            archiveTmpPath,
            BackupService.ARCHIVE_TEMPLATE,
            compressedStagingPath,
            this.#encState,
            manifest.meta
          ).finally(async () => {
            await IOUtils.remove(compressedStagingPath);
          });

          // Record the size of the complete single-file archive
          let archiveSizeKilobytes = await BackupResource.getFileSize(
            archiveTmpPath
          );
          let archiveSizeBytesNearestMebibyte = MeasurementUtils.fuzzByteSize(
            archiveSizeKilobytes * BYTES_IN_KILOBYTE,
            1 * BYTES_IN_MEBIBYTE
          );
          lazy.logConsole.debug(
            "backup archive size in bytes: " + archiveSizeBytesNearestMebibyte
          );

          Glean.browserBackup.compressedArchiveSize.accumulate(
            archiveSizeBytesNearestMebibyte / BYTES_IN_MEBIBYTE
          );

          let archivePath = await this.finalizeSingleFileArchive(
            archiveTmpPath,
            archiveDestFolderPath,
            manifest.meta
          );

          let nowSeconds = Math.floor(Date.now() / 1000);
          Services.prefs.setIntPref(
            LAST_BACKUP_TIMESTAMP_PREF_NAME,
            nowSeconds
          );
          this.#_state.lastBackupDate = nowSeconds;
          Glean.browserBackup.totalBackupTime.stopAndAccumulate(backupTimer);

          return { manifest, archivePath };
        } catch {
          Glean.browserBackup.totalBackupTime.cancel(backupTimer);
          return null;
        } finally {
          this.#backupInProgress = false;
        }
      }
    );
  }

  /**
   * Generates a string from a Date in the form of:
   *
   * YYYYMMDD-HHMM
   *
   * @param {Date} date
   *   The date to convert into the archive date suffix.
   * @returns {string}
   */
  generateArchiveDateSuffix(date) {
    let year = date.getFullYear().toString();

    // In all cases, months or days with single digits are expected to start
    // with a 0.

    // Note that getMonth() is 0-indexed for some reason, so we increment by 1.
    let month = `${date.getMonth() + 1}`.padStart(2, "0");

    let day = `${date.getDate()}`.padStart(2, "0");
    let hours = `${date.getHours()}`.padStart(2, "0");
    let minutes = `${date.getMinutes()}`.padStart(2, "0");

    return `${year}${month}${day}-${hours}${minutes}`;
  }

  /**
   * Moves the single-file archive into its configured location with a filename
   * that is sanitized and contains a timecode. This also removes any existing
   * single-file archives in that same folder after the move completes.
   *
   * @param {string} sourcePath
   *   The file system location of the single-file archive prior to the move.
   * @param {string} destFolder
   *   The folder that the single-file archive is configured to be eventually
   *   written to.
   * @param {object} metadata
   *   The metadata for the backup. See the BackupManifest schema for details.
   * @returns {Promise<string>}
   *   Resolves with the path that the single-file archive was moved to.
   */
  async finalizeSingleFileArchive(sourcePath, destFolder, metadata) {
    let archiveDateSuffix = this.generateArchiveDateSuffix(
      new Date(metadata.date)
    );

    let existingChildren = await IOUtils.getChildren(destFolder);

    const FILENAME_PREFIX = `${BackupService.BACKUP_FILE_NAME}_${metadata.profileName}`;
    const FILENAME = `${FILENAME_PREFIX}_${archiveDateSuffix}.html`;
    let destPath = PathUtils.join(destFolder, FILENAME);
    lazy.logConsole.log("Moving single-file archive to ", destPath);
    await IOUtils.move(sourcePath, destPath);

    Services.prefs.setStringPref(LAST_BACKUP_FILE_NAME_PREF_NAME, FILENAME);
    // It is expected that our caller will call stateUpdate(), so we skip doing
    // that here. This is done via the backupInProgress setter in createBackup.
    this.#_state.lastBackupFileName = FILENAME;

    for (let childFilePath of existingChildren) {
      let childFileName = PathUtils.filename(childFilePath);
      // We check both the prefix and the suffix, because the prefix encodes
      // the profile name in it. If there are other profiles from the same
      // application performing backup, we don't want to accidentally remove
      // those.
      if (
        childFileName.startsWith(FILENAME_PREFIX) &&
        childFileName.endsWith(".html")
      ) {
        if (childFileName == FILENAME) {
          // Since filenames don't include seconds, this might occur if a
          // backup was created seconds after the last one during the same
          // minute. That tends not to happen in practice, but might occur
          // during testing, in which case, we'll skip clearing this file.
          lazy.logConsole.warn(
            "Collided with a pre-existing archive name, so not clearing: ",
            FILENAME
          );
          continue;
        }
        lazy.logConsole.debug("Getting rid of ", childFilePath);
        await IOUtils.remove(childFilePath);
      }
    }

    return destPath;
  }

  /**
   * Constructs the staging folder for the backup in the passed in backup
   * folder. If a pre-existing staging folder exists, it will be cleared out.
   *
   * @param {string} backupDirPath
   *   The path to the backup folder.
   * @returns {Promise<string>}
   *   The path to the empty staging folder.
   */
  async #prepareStagingFolder(backupDirPath) {
    let stagingPath = PathUtils.join(backupDirPath, "staging");
    lazy.logConsole.debug("Checking for pre-existing staging folder");
    if (await IOUtils.exists(stagingPath)) {
      // A pre-existing staging folder exists. A previous backup attempt must
      // have failed or been interrupted. We'll clear it out.
      lazy.logConsole.warn("A pre-existing staging folder exists. Clearing.");
      await IOUtils.remove(stagingPath, { recursive: true });
    }
    await IOUtils.makeDirectory(stagingPath);

    return stagingPath;
  }

  /**
   * Compresses a staging folder into a Zip file. If a pre-existing Zip file
   * for a staging folder resides in destFolderPath, it is overwritten. The
   * Zip file will have the same name as the stagingPath folder, with `.zip`
   * as the extension.
   *
   * @param {string} stagingPath
   *   The path to the staging folder to be compressed.
   * @param {string} destFolderPath
   *   The parent folder to write the Zip file to.
   * @returns {Promise<string>}
   *   Resolves with the path to the created Zip file.
   */
  async #compressStagingFolder(stagingPath, destFolderPath) {
    const PR_RDWR = 0x04;
    const PR_CREATE_FILE = 0x08;
    const PR_TRUNCATE = 0x20;

    let archivePath = PathUtils.join(
      destFolderPath,
      `${PathUtils.filename(stagingPath)}.zip`
    );
    let archiveFile = await IOUtils.getFile(archivePath);

    let writer = new lazy.ZipWriter(
      archiveFile,
      PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE
    );

    lazy.logConsole.log("Compressing staging folder to ", archivePath);
    let rootPathNSIFile = await IOUtils.getDirectory(stagingPath);
    await this.#compressChildren(rootPathNSIFile, stagingPath, writer);
    await new Promise(resolve => {
      let observer = {
        onStartRequest(_request) {
          lazy.logConsole.debug("Starting to write out archive file");
        },
        onStopRequest(_request, status) {
          lazy.logConsole.log("Done writing archive file");
          resolve(status);
        },
      };
      writer.processQueue(observer, null);
    });
    writer.close();

    return archivePath;
  }

  /**
   * A helper function for #compressStagingFolder that iterates through a
   * directory, and adds each file to a nsIZipWriter. For each directory it
   * finds, it recurses.
   *
   * @param {nsIFile} rootPathNSIFile
   *   An nsIFile pointing at the root of the folder being compressed.
   * @param {string} parentPath
   *   The path to the folder whose children should be iterated.
   * @param {nsIZipWriter} writer
   *   The writer to add all of the children to.
   * @returns {Promise<undefined>}
   */
  async #compressChildren(rootPathNSIFile, parentPath, writer) {
    let children = await IOUtils.getChildren(parentPath);
    for (let childPath of children) {
      let childState = await IOUtils.stat(childPath);
      if (childState.type == "directory") {
        await this.#compressChildren(rootPathNSIFile, childPath, writer);
      } else {
        let childFile = await IOUtils.getFile(childPath);
        // nsIFile.getRelativePath returns paths using the "/" separator,
        // regardless of which platform we're on. That's handy, because this
        // is the same separator that nsIZipWriter expects for entries.
        let pathRelativeToRoot = childFile.getRelativePath(rootPathNSIFile);
        writer.addEntryFile(
          pathRelativeToRoot,
          BackupService.COMPRESSION_LEVEL,
          childFile,
          true
        );
      }
    }
  }

  /**
   * Decompressed a compressed recovery file into recoveryFolderDestPath.
   *
   * @param {string} recoveryFilePath
   *   The path to the compressed recovery file to decompress.
   * @param {string} recoveryFolderDestPath
   *   The path to the folder that the compressed recovery file should be
   *   decompressed within.
   * @returns {Promise<undefined>}
   */
  async decompressRecoveryFile(recoveryFilePath, recoveryFolderDestPath) {
    let recoveryFile = await IOUtils.getFile(recoveryFilePath);
    let recoveryArchive = new lazy.ZipReader(recoveryFile);
    lazy.logConsole.log(
      "Decompressing recovery folder to ",
      recoveryFolderDestPath
    );
    try {
      // null is passed to test if we're meant to CRC test the entire
      // ZIP file. If an exception is thrown, this means we failed the CRC
      // check. See the nsIZipReader.idl documentation for details.
      recoveryArchive.test(null);
    } catch (e) {
      recoveryArchive.close();
      lazy.logConsole.error("Compressed recovery file was corrupt.");
      await IOUtils.remove(recoveryFilePath, {
        retryReadonly: true,
      });
      throw new BackupError("Corrupt archive.", ERRORS.CORRUPTED_ARCHIVE);
    }

    await this.#decompressChildren(recoveryFolderDestPath, "", recoveryArchive);
    recoveryArchive.close();
  }

  /**
   * A helper method that recursively decompresses any children within a folder
   * within a compressed archive.
   *
   * @param {string} rootPath
   *   The path to the root folder that is being decompressed into.
   * @param {string} parentEntryName
   *   The name of the parent folder within the compressed archive that is
   *   having its children decompressed.
   * @param {nsIZipReader} reader
   *   The nsIZipReader for the compressed archive.
   * @returns {Promise<undefined>}
   */
  async #decompressChildren(rootPath, parentEntryName, reader) {
    // nsIZipReader.findEntries has an interesting querying language that is
    // documented in the nsIZipReader IDL file, in case you're curious about
    // what these symbols mean.
    let childEntryNames = reader.findEntries(
      parentEntryName + "?*~" + parentEntryName + "?*/?*"
    );

    for (let childEntryName of childEntryNames) {
      let childEntry = reader.getEntry(childEntryName);
      if (childEntry.isDirectory) {
        await this.#decompressChildren(rootPath, childEntryName, reader);
      } else {
        let inputStream = reader.getInputStream(childEntryName);
        // ZIP files all use `/` as their path separators, regardless of
        // platform.
        let fileNameParts = childEntryName.split("/");
        let outputFilePath = PathUtils.join(rootPath, ...fileNameParts);
        let outputFile = await IOUtils.getFile(outputFilePath);
        let outputStream = Cc[
          "@mozilla.org/network/file-output-stream;1"
        ].createInstance(Ci.nsIFileOutputStream);

        outputStream.init(
          outputFile,
          -1,
          -1,
          Ci.nsIFileOutputStream.DEFER_OPEN
        );

        await new Promise(resolve => {
          lazy.logConsole.debug("Writing ", outputFilePath);
          lazy.NetUtil.asyncCopy(inputStream, outputStream, () => {
            lazy.logConsole.debug("Done writing ", outputFilePath);
            outputStream.close();
            resolve();
          });
        });
      }
    }
  }

  /**
   * Given a URI to an HTML template for the single-file backup archive,
   * produces the static markup that will then be used as the beginning of that
   * single-file backup archive.
   *
   * @param {string} templateURI
   *   A URI pointing at a template for the HTML content for the page. This is
   *   what is visible if the file is loaded in a web browser.
   * @param {boolean} isEncrypted
   *   True if the template should indicate that the backup is encrypted.
   * @param {object} backupMetadata
   *   The metadata for the backup, which is also stored in the backup manifest
   *   of the compressed backup snapshot.
   * @returns {Promise<string>}
   */
  async renderTemplate(templateURI, isEncrypted, backupMetadata) {
    const ARCHIVE_STYLES = "chrome://browser/content/backup/archive.css";
    const ARCHIVE_SCRIPT = "chrome://browser/content/backup/archive.js";
    const LOGO = "chrome://branding/content/icon128.png";

    let templateResponse = await fetch(templateURI);
    let templateString = await templateResponse.text();
    let templateDOM = new DOMParser().parseFromString(
      templateString,
      "text/html"
    );

    // Set the lang attribute on the <html> element
    templateDOM.documentElement.setAttribute(
      "lang",
      Services.locale.appLocaleAsBCP47
    );

    let downloadLink = templateDOM.querySelector("#download-moz-browser");
    downloadLink.href = await this.resolveDownloadLink(
      AppConstants.MOZ_UPDATE_CHANNEL
    );

    let supportLinkHref =
      Services.urlFormatter.formatURLPref("app.support.baseURL") +
      "recover-from-backup";
    let supportLink = templateDOM.querySelector("#support-link");
    supportLink.href = supportLinkHref;

    // Now insert the logo as a dataURL, since we want the single-file backup
    // archive to be entirely self-contained.
    let logoResponse = await fetch(LOGO);
    let logoBlob = await logoResponse.blob();
    let logoDataURL = await new Promise((resolve, reject) => {
      let reader = new FileReader();
      reader.addEventListener("load", () => resolve(reader.result));
      reader.addEventListener("error", reject);
      reader.readAsDataURL(logoBlob);
    });

    let logoNode = templateDOM.querySelector("#logo");
    logoNode.src = logoDataURL;

    let encStateNode = templateDOM.querySelector("#encryption-state");
    lazy.gDOMLocalization.setAttributes(
      encStateNode,
      isEncrypted
        ? "backup-file-encryption-state-encrypted"
        : "backup-file-encryption-state-not-encrypted"
    );

    let lastBackedUpNode = templateDOM.querySelector("#last-backed-up");
    lazy.gDOMLocalization.setArgs(lastBackedUpNode, {
      // It's very unlikely that backupMetadata.date isn't a valid Date string,
      // but if it _is_, then Fluent will cause us to crash in debug builds.
      // We fallback to the current date if all else fails.
      date: new Date(backupMetadata.date).getTime() || new Date().getTime(),
    });

    let creationDeviceNode = templateDOM.querySelector("#creation-device");
    lazy.gDOMLocalization.setArgs(creationDeviceNode, {
      machineName: backupMetadata.machineName,
    });

    try {
      await lazy.gDOMLocalization.translateFragment(
        templateDOM.documentElement
      );
    } catch (_) {
      // This shouldn't happen, but we don't want a missing locale string to
      // cause backup creation to fail.
    }

    // We have to insert styles and scripts after we serialize to XML, otherwise
    // the XMLSerializer will escape things like descendent selectors in CSS
    // with &gt;.
    let stylesResponse = await fetch(ARCHIVE_STYLES);
    let scriptResponse = await fetch(ARCHIVE_SCRIPT);

    // These days, we don't really support CSS preprocessor directives, so we
    // can't ifdef out the MPL license header in styles before writing it into
    // the archive file. Instead, we'll ensure that the license header is there,
    // and then manually remove it here at runtime.
    let stylesText = await stylesResponse.text();
    const MPL_LICENSE = `/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */`;
    if (!stylesText.includes(MPL_LICENSE)) {
      throw new BackupError(
        "Expected the MPL license block within archive.css",
        ERRORS.UNKNOWN
      );
    }

    stylesText = stylesText.replace(MPL_LICENSE, "");

    let serializer = new XMLSerializer();
    return serializer
      .serializeToString(templateDOM)
      .replace("{{styles}}", stylesText)
      .replace("{{script}}", await scriptResponse.text());
  }

  /**
   * Creates a portable, potentially encrypted single-file archive containing
   * a compressed backup snapshot. The single-file archive is a specially
   * crafted HTML file that embeds the compressed backup snapshot and
   * backup metadata.
   *
   * @param {string} archivePath
   *   The path to write the single-file archive to.
   * @param {string} templateURI
   *   A URI pointing at a template for the HTML content for the page. This is
   *   what is visible if the file is loaded in a web browser.
   * @param {string} compressedBackupSnapshotPath
   *   The path on the file system where the compressed backup snapshot exists.
   * @param {ArchiveEncryptionState|null} encState
   *   The ArchiveEncryptionState to encrypt the backup with, if encryption is
   *   enabled. If null is passed, the backup will not be encrypted.
   * @param {object} backupMetadata
   *   The metadata for the backup, which is also stored in the backup manifest
   *   of the compressed backup snapshot.
   * @param {object} options
   *   Options to pass to the worker, mainly for testing.
   * @param {object} [options.chunkSize=ArchiveUtils.ARCHIVE_CHUNK_MAX_BYTES_SIZE]
   *   The chunk size to break the bytes into.
   */
  async createArchive(
    archivePath,
    templateURI,
    compressedBackupSnapshotPath,
    encState,
    backupMetadata,
    options = {}
  ) {
    let markup = await this.renderTemplate(
      templateURI,
      !!encState,
      backupMetadata
    );

    let worker = new lazy.BasePromiseWorker(
      "resource:///modules/backup/Archive.worker.mjs",
      { type: "module" }
    );
    worker.ExceptionHandlers[BackupError.name] = BackupError.fromMsg;

    let chunkSize =
      options.chunkSize || lazy.ArchiveUtils.ARCHIVE_CHUNK_MAX_BYTES_SIZE;

    try {
      let encryptionArgs = encState
        ? {
            publicKey: encState.publicKey,
            salt: encState.salt,
            nonce: encState.nonce,
            backupAuthKey: encState.backupAuthKey,
            wrappedSecrets: encState.wrappedSecrets,
          }
        : null;

      await worker
        .post("constructArchive", [
          {
            archivePath,
            markup,
            backupMetadata,
            compressedBackupSnapshotPath,
            encryptionArgs,
            chunkSize,
          },
        ])
        .catch(e => {
          lazy.logConsole.error(e);
          if (!(e instanceof BackupError)) {
            throw new BackupError("Failed to create archive", ERRORS.UNKNOWN);
          }
          throw e;
        });
    } finally {
      worker.terminate();
    }
  }

  /**
   * Constructs an nsIChannel that serves the bytes from an nsIInputStream -
   * specifically, a nsIInputStream of bytes being streamed from a file.
   *
   * @see BackupService.#extractMetadataFromArchive()
   * @param {nsIInputStream} inputStream
   *   The nsIInputStream to create the nsIChannel for.
   * @param {string} contentType
   *   The content type for the nsIChannel. This is provided by
   *   BackupService.#extractMetadataFromArchive().
   * @returns {nsIChannel}
   */
  #createExtractionChannel(inputStream, contentType) {
    let uri = "http://localhost";
    let httpChan = lazy.NetUtil.newChannel({
      uri,
      loadUsingSystemPrincipal: true,
    });

    let channel = Cc["@mozilla.org/network/input-stream-channel;1"]
      .createInstance(Ci.nsIInputStreamChannel)
      .QueryInterface(Ci.nsIChannel);

    channel.setURI(httpChan.URI);
    channel.loadInfo = httpChan.loadInfo;

    channel.contentStream = inputStream;
    channel.contentType = contentType;
    return channel;
  }

  /**
   * A helper for BackupService.extractCompressedSnapshotFromArchive() that
   * reads in the JSON block from the MIME message embedded within an
   * archiveFile.
   *
   * @see BackupService.extractCompressedSnapshotFromArchive()
   * @param {nsIFile} archiveFile
   *   The file to read the MIME message out from.
   * @param {number} startByteOffset
   *   The start byte offset of the MIME message.
   * @param {string} contentType
   *   The Content-Type of the MIME message.
   * @returns {Promise<object>}
   */
  async #extractJSONFromArchive(archiveFile, startByteOffset, contentType) {
    let fileInputStream = Cc[
      "@mozilla.org/network/file-input-stream;1"
    ].createInstance(Ci.nsIFileInputStream);
    fileInputStream.init(
      archiveFile,
      -1,
      -1,
      Ci.nsIFileInputStream.CLOSE_ON_EOF
    );
    fileInputStream.seek(Ci.nsISeekableStream.NS_SEEK_SET, startByteOffset);

    const EXPECTED_CONTENT_TYPE = "application/json";

    let extractionChannel = this.#createExtractionChannel(
      fileInputStream,
      contentType
    );
    let textDecoder = new TextDecoder();
    return new Promise((resolve, reject) => {
      let streamConv = Cc["@mozilla.org/streamConverters;1"].getService(
        Ci.nsIStreamConverterService
      );
      let multipartListenerForJSON = {
        /**
         * True once we've found an attachment matching our
         * EXPECTED_CONTENT_TYPE. Once this is true, bytes flowing into
         * onDataAvailable will be enqueued through the controller.
         *
         * @type {boolean}
         */
        _enabled: false,

        /**
         * True once onStopRequest has been called once the listener is enabled.
         * After this, the listener will not attempt to read any data passed
         * to it through onDataAvailable.
         *
         * @type {boolean}
         */
        _done: false,

        /**
         * A buffer with which we will cobble together the JSON string that
         * will get parsed once the attachment finishes being read in.
         *
         * @type {string}
         */
        _buffer: "",

        QueryInterface: ChromeUtils.generateQI([
          "nsIStreamListener",
          "nsIRequestObserver",
          "nsIMultiPartChannelListener",
        ]),

        /**
         * Called when we begin to load an attachment from the MIME message.
         *
         * @param {nsIRequest} request
         *   The request corresponding to the source of the data.
         */
        onStartRequest(request) {
          if (!(request instanceof Ci.nsIChannel)) {
            throw Components.Exception(
              "onStartRequest expected an nsIChannel request",
              Cr.NS_ERROR_UNEXPECTED
            );
          }
          this._enabled = request.contentType == EXPECTED_CONTENT_TYPE;
        },

        /**
         * Called when data is flowing in for an attachment.
         *
         * @param {nsIRequest} request
         *   The request corresponding to the source of the data.
         * @param {nsIInputStream} stream
         *   The input stream containing the data chunk.
         * @param {number} offset
         *   The number of bytes that were sent in previous onDataAvailable
         *   calls for this request. In other words, the sum of all previous
         *   count parameters.
         * @param {number} count
         *   The number of bytes available in the stream
         */
        onDataAvailable(request, stream, offset, count) {
          if (!this._enabled) {
            // We don't care about this data, just move on.
            return;
          }

          let binStream = new lazy.BinaryInputStream(stream);
          let arrBuffer = new ArrayBuffer(count);
          binStream.readArrayBuffer(count, arrBuffer);
          let jsonBytes = new Uint8Array(arrBuffer);
          this._buffer += textDecoder.decode(jsonBytes);
        },

        /**
         * Called when the load of an attachment finishes.
         */
        onStopRequest() {
          if (this._enabled && !this._done) {
            this._enabled = false;
            this._done = true;

            try {
              let archiveMetadata = JSON.parse(this._buffer);
              resolve(archiveMetadata);
            } catch (e) {
              reject(
                new BackupError(
                  "Could not parse archive metadata.",
                  ERRORS.CORRUPTED_ARCHIVE
                )
              );
            }
            // No need to load anything else - abort reading in more
            // attachments.
            throw Components.Exception(
              "Got JSON block. Aborting further reads.",
              Cr.NS_BINDING_ABORTED
            );
          }
        },

        onAfterLastPart() {
          if (!this._done) {
            // We finished reading the parts before we found the JSON block, so
            // the JSON block is missing.
            reject(
              new BackupError(
                "Could not find JSON block.",
                ERRORS.CORRUPTED_ARCHIVE
              )
            );
          }
        },
      };
      let conv = streamConv.asyncConvertData(
        "multipart/mixed",
        "*/*",
        multipartListenerForJSON,
        null
      );

      extractionChannel.asyncOpen(conv);
    });
  }

  /**
   * A helper for BackupService.#extractCompressedSnapshotFromArchive that
   * constructs a BinaryReadableStream for a single-file archive on the
   * file system. The BinaryReadableStream will be used to read out the binary
   * attachment from the archive.
   *
   * @param {nsIFile} archiveFile
   *   The single-file archive to create the BinaryReadableStream for.
   * @param {number} startByteOffset
   *   The start byte offset of the MIME message.
   * @param {string} contentType
   *   The Content-Type of the MIME message.
   * @returns {ReadableStream}
   */
  async createBinaryReadableStream(archiveFile, startByteOffset, contentType) {
    let fileInputStream = Cc[
      "@mozilla.org/network/file-input-stream;1"
    ].createInstance(Ci.nsIFileInputStream);
    fileInputStream.init(
      archiveFile,
      -1,
      -1,
      Ci.nsIFileInputStream.CLOSE_ON_EOF
    );
    fileInputStream.seek(Ci.nsISeekableStream.NS_SEEK_SET, startByteOffset);

    let extractionChannel = this.#createExtractionChannel(
      fileInputStream,
      contentType
    );

    return new ReadableStream(new BinaryReadableStream(extractionChannel));
  }

  /**
   * @typedef {object} SampleArchiveResult
   * @property {boolean} isEncrypted
   *   True if the archive claims to be encrypted, and has the necessary data
   *   within the JSON block to attempt to initialize an ArchiveDecryptor.
   * @property {number} startByteOffset
   *   The start byte offset of the MIME message.
   * @property {string} contentType
   *   The Content-Type of the MIME message.
   * @property {object} archiveJSON
   *   The deserialized JSON block from the archive. See the ArchiveJSONBlock
   *   schema for details of its structure.
   */

  /**
   * Reads from a file to determine if it seems to be a backup archive, and if
   * so, resolves with some information about the archive without actually
   * unpacking it. The returned Promise may reject if the file does not appear
   * to be a backup archive, or the backup archive appears to have been
   * corrupted somehow.
   *
   * @param {string} archivePath
   *   The path to the archive file to sample.
   * @returns {Promise<SampleArchiveResult, Error>}
   */
  async sampleArchive(archivePath) {
    let worker = new lazy.BasePromiseWorker(
      "resource:///modules/backup/Archive.worker.mjs",
      { type: "module" }
    );
    worker.ExceptionHandlers[BackupError.name] = BackupError.fromMsg;

    if (!(await IOUtils.exists(archivePath))) {
      throw new BackupError(
        "Archive file does not exist at path " + archivePath,
        ERRORS.UNKNOWN
      );
    }

    try {
      let { startByteOffset, contentType } = await worker
        .post("parseArchiveHeader", [archivePath])
        .catch(e => {
          lazy.logConsole.error(e);
          if (!(e instanceof BackupError)) {
            throw new BackupError(
              "Failed to parse archive header",
              ERRORS.FILE_SYSTEM_ERROR
            );
          }
          throw e;
        });
      let archiveFile = await IOUtils.getFile(archivePath);
      let archiveJSON;
      try {
        archiveJSON = await this.#extractJSONFromArchive(
          archiveFile,
          startByteOffset,
          contentType
        );

        if (!archiveJSON.version) {
          throw new BackupError(
            "Missing version in the archive JSON block.",
            ERRORS.CORRUPTED_ARCHIVE
          );
        }
        if (archiveJSON.version > lazy.ArchiveUtils.SCHEMA_VERSION) {
          throw new BackupError(
            `Archive JSON block is a version newer than we can interpret: ${archiveJSON.version}`,
            ERRORS.UNSUPPORTED_BACKUP_VERSION
          );
        }

        let archiveJSONSchema = await BackupService.getSchemaForVersion(
          SCHEMAS.ARCHIVE_JSON_BLOCK,
          archiveJSON.version
        );

        let manifestSchema = await BackupService.getSchemaForVersion(
          SCHEMAS.BACKUP_MANIFEST,
          archiveJSON.version
        );

        let validator = new lazy.JsonSchema.Validator(archiveJSONSchema);
        validator.addSchema(manifestSchema);

        let schemaValidationResult = validator.validate(archiveJSON);
        if (!schemaValidationResult.valid) {
          lazy.logConsole.error(
            "Archive JSON block does not conform to schema:",
            archiveJSON,
            archiveJSONSchema,
            schemaValidationResult
          );

          // TODO: Collect telemetry for this case. (bug 1891817)
          throw new BackupError(
            `Archive JSON block does not conform to schema version ${archiveJSON.version}`,
            ERRORS.CORRUPTED_ARCHIVE
          );
        }
      } catch (e) {
        lazy.logConsole.error(e);
        throw e;
      }

      lazy.logConsole.debug("Read out archive JSON: ", archiveJSON);

      return {
        isEncrypted: !!archiveJSON.encConfig,
        startByteOffset,
        contentType,
        archiveJSON,
      };
    } catch (e) {
      lazy.logConsole.error(e);
      throw e;
    } finally {
      worker.terminate();
    }
  }

  /**
   * Attempts to extract the compressed backup snapshot from a single-file
   * archive, and write the extracted file to extractionDestPath. This may
   * reject if the single-file archive appears malformed or cannot be
   * properly decrypted. If the backup was encrypted, a native nsIOSKeyStore
   * is also initialized with label BackupService.RECOVERY_OSKEYSTORE_LABEL
   * with the secret used on the original backup machine. Callers are
   * responsible for clearing this secret after any decryptions with it are
   * completed.
   *
   * NOTE: Currently, this base64 decoding currently occurs on the main thread.
   * We may end up moving all of this into the Archive Worker if we can modify
   * IOUtils to allow writing via a stream.
   *
   * @param {string} archivePath
   *   The single-file archive that contains the backup.
   * @param {string} extractionDestPath
   *   The path to write the extracted file to.
   * @param {string} [recoveryCode=null]
   *   The recovery code to decrypt an encrypted backup with.
   * @returns {Promise<undefined, Error>}
   */
  async extractCompressedSnapshotFromArchive(
    archivePath,
    extractionDestPath,
    recoveryCode = null
  ) {
    let { isEncrypted, startByteOffset, contentType, archiveJSON } =
      await this.sampleArchive(archivePath);

    let decryptor = null;
    if (isEncrypted) {
      if (!recoveryCode) {
        throw new BackupError(
          "A recovery code is required to decrypt this archive.",
          ERRORS.UNAUTHORIZED
        );
      }
      decryptor = await lazy.ArchiveDecryptor.initialize(
        recoveryCode,
        archiveJSON
      );
    }

    await IOUtils.remove(extractionDestPath, { ignoreAbsent: true });

    let archiveFile = await IOUtils.getFile(archivePath);
    let archiveStream = await this.createBinaryReadableStream(
      archiveFile,
      startByteOffset,
      contentType
    );

    let binaryDecoder = new TransformStream(
      new DecoderDecryptorTransformer(decryptor)
    );
    let fileWriter = new WritableStream(
      new FileWriterStream(extractionDestPath, decryptor)
    );
    await archiveStream.pipeThrough(binaryDecoder).pipeTo(fileWriter);

    if (decryptor) {
      await lazy.nativeOSKeyStore.asyncRecoverSecret(
        BackupService.RECOVERY_OSKEYSTORE_LABEL,
        decryptor.OSKeyStoreSecret
      );
    }
  }

  /**
   * Renames the staging folder to an ISO 8601 date string with dashes replacing colons and fractional seconds stripped off.
   * The ISO date string should be formatted from YYYY-MM-DDTHH:mm:ss.sssZ to YYYY-MM-DDTHH-mm-ssZ
   *
   * @param {string} stagingPath
   *   The path to the populated staging folder.
   * @returns {Promise<string|null>}
   *   The path to the renamed staging folder, or null if the stagingPath was
   *   not pointing to a valid folder.
   */
  async #finalizeStagingFolder(stagingPath) {
    if (!(await IOUtils.exists(stagingPath))) {
      // If we somehow can't find the specified staging folder, cancel this step.
      lazy.logConsole.error(
        `Failed to finalize staging folder. Cannot find ${stagingPath}.`
      );
      return null;
    }

    try {
      lazy.logConsole.debug("Finalizing and renaming staging folder");
      let currentDateISO = new Date().toISOString();
      // First strip the fractional seconds
      let dateISOStripped = currentDateISO.replace(/\.\d+\Z$/, "Z");
      // Now replace all colons with dashes
      let dateISOFormatted = dateISOStripped.replaceAll(":", "-");

      let stagingPathParent = PathUtils.parent(stagingPath);
      let renamedBackupPath = PathUtils.join(
        stagingPathParent,
        dateISOFormatted
      );
      await IOUtils.move(stagingPath, renamedBackupPath);

      let existingBackups = await IOUtils.getChildren(stagingPathParent);

      /**
       * Bug 1892532: for now, we only support a single backup file.
       * If there are other pre-existing backup folders, delete them - but don't
       * delete anything that doesn't match the backup folder naming scheme.
       */
      let expectedFormatRegex = /\d{4}(-\d{2}){2}T(\d{2}-){2}\d{2}Z/;
      for (let existingBackupPath of existingBackups) {
        if (
          existingBackupPath !== renamedBackupPath &&
          existingBackupPath.match(expectedFormatRegex)
        ) {
          await IOUtils.remove(existingBackupPath, {
            recursive: true,
          });
        }
      }
      return renamedBackupPath;
    } catch (e) {
      lazy.logConsole.error(
        `Something went wrong while finalizing the staging folder. ${e}`
      );
      throw new BackupError(
        "Failed to finalize staging folder",
        ERRORS.FILE_SYSTEM_ERROR
      );
    }
  }

  /**
   * Creates and resolves with a backup manifest object with an empty resources
   * property. See the BackupManifest schema for the specific shape of the
   * returned manifest object.
   *
   * @returns {Promise<object>}
   */
  async #createBackupManifest() {
    let profileSvc = Cc["@mozilla.org/toolkit/profile-service;1"].getService(
      Ci.nsIToolkitProfileService
    );
    let profileName;
    if (!profileSvc.currentProfile) {
      // We're probably running on a local build or in some special configuration.
      // Let's pull in a profile name from the profile directory.
      let profileFolder = PathUtils.split(PathUtils.profileDir).at(-1);
      profileName = profileFolder.substring(profileFolder.indexOf(".") + 1);
    } else {
      profileName = profileSvc.currentProfile.name;
    }

    let meta = {
      date: new Date().toISOString(),
      appName: AppConstants.MOZ_APP_NAME,
      appVersion: AppConstants.MOZ_APP_VERSION,
      buildID: AppConstants.MOZ_BUILDID,
      profileName,
      machineName: lazy.fxAccounts.device.getLocalName(),
      osName: Services.sysinfo.getProperty("name"),
      osVersion: Services.sysinfo.getProperty("version"),
      legacyClientID: await lazy.ClientID.getClientID(),
      profileGroupID: await lazy.ClientID.getProfileGroupID(),
    };

    let fxaState = lazy.UIState.get();
    if (fxaState.status == lazy.UIState.STATUS_SIGNED_IN) {
      meta.accountID = fxaState.uid;
      meta.accountEmail = fxaState.email;
    }

    return {
      version: lazy.ArchiveUtils.SCHEMA_VERSION,
      meta,
      resources: {},
    };
  }

  /**
   * Given a backup archive at archivePath, this method does the
   * following:
   *
   * 1. Potentially decrypts, and then extracts the compressed backup snapshot
   *    from the archive to a file named BackupService.RECOVERY_ZIP_FILE_NAME in
   *    the PROFILE_FOLDER_NAME folder.
   * 2. Decompresses that file into a subdirectory of PROFILE_FOLDER_NAME named
   *    "recovery".
   * 3. Deletes the BackupService.RECOVERY_ZIP_FILE_NAME file.
   * 4. Calls into recoverFromSnapshotFolder on the decompressed "recovery"
   *    folder.
   * 5. Optionally launches the newly created profile.
   * 6. Returns the name of the newly created profile directory.
   *
   * @see BackupService.recoverFromSnapshotFolder
   * @param {string} archivePath
   *   The path to the single-file backup archive on the file system.
   * @param {string|null} recoveryCode
   *   The recovery code to use to attempt to decrypt the archive if it was
   *   encrypted.
   * @param {boolean} [shouldLaunch=false]
   *   An optional argument that specifies whether an instance of the app
   *   should be launched with the newly recovered profile after recovery is
   *   complete.
   * @param {boolean} [profilePath=PathUtils.profileDir]
   *   The profile path where the recovery files will be written to within the
   *   PROFILE_FOLDER_NAME. This is only used for testing.
   * @param {string} [profileRootPath=null]
   *   An optional argument that specifies the root directory where the new
   *   profile directory should be created. If not provided, the default
   *   profile root directory will be used. This is primarily meant for
   *   testing.
   * @returns {Promise<nsIToolkitProfile>}
   *   The nsIToolkitProfile that was created for the recovered profile.
   * @throws {Exception}
   *   In the event that unpacking the archive, decompressing the snapshot, or
   *   recovery from the snapshot somehow failed.
   */
  async recoverFromBackupArchive(
    archivePath,
    recoveryCode = null,
    shouldLaunch = false,
    profilePath = PathUtils.profileDir,
    profileRootPath = null
  ) {
    // No concurrent recoveries.
    if (this.#recoveryInProgress) {
      lazy.logConsole.warn("Recovery attempt already in progress");
      return null;
    }

    try {
      this.#recoveryInProgress = true;
      const RECOVERY_FILE_DEST_PATH = PathUtils.join(
        profilePath,
        BackupService.PROFILE_FOLDER_NAME,
        BackupService.RECOVERY_ZIP_FILE_NAME
      );
      await this.extractCompressedSnapshotFromArchive(
        archivePath,
        RECOVERY_FILE_DEST_PATH,
        recoveryCode
      );

      let encState = null;
      if (recoveryCode) {
        // We were passed a recovery code and made it to this line. That implies
        // that the backup was encrypted, and the recovery code was the correct
        // one to decrypt it. We now generate a new ArchiveEncryptionState with
        // that recovery code to write into the recovered profile.
        ({ instance: encState } = await lazy.ArchiveEncryptionState.initialize(
          recoveryCode
        ));
      }

      const RECOVERY_FOLDER_DEST_PATH = PathUtils.join(
        profilePath,
        BackupService.PROFILE_FOLDER_NAME,
        "recovery"
      );
      await this.decompressRecoveryFile(
        RECOVERY_FILE_DEST_PATH,
        RECOVERY_FOLDER_DEST_PATH
      );

      // Now that we've decompressed it, reclaim some disk space by getting rid of
      // the ZIP file.
      try {
        await IOUtils.remove(RECOVERY_FILE_DEST_PATH);
      } catch (_) {
        lazy.logConsole.warn("Could not remove ", RECOVERY_FILE_DEST_PATH);
      }
      try {
        // We're using a try/finally here to clean up the temporary OSKeyStore.
        // We need to make sure that cleanup occurs _after_ the recovery has
        // either fully succeeded, or fully failed. We await the return value
        // of recoverFromSnapshotFolder so that the finally will not execute
        // until after recoverFromSnapshotFolder has finished resolving or
        // rejecting.
        let newProfile = await this.recoverFromSnapshotFolder(
          RECOVERY_FOLDER_DEST_PATH,
          shouldLaunch,
          profileRootPath,
          encState
        );
        return newProfile;
      } finally {
        // If we had decrypted a backup, we would have created the temporary
        // recovery OSKeyStore row with the label
        // BackupService.RECOVERY_OSKEYSTORE_LABEL, which we will now delete,
        // no matter if we succeeded or failed to recover.
        //
        // Note that according to nsIOSKeyStore, this is a no-op in the event that
        // no secret exists at BackupService.RECOVERY_OSKEYSTORE_LABEL, so we're
        // fine to do this even if we were recovering from an unencrypted
        // backup.
        if (recoveryCode) {
          await lazy.nativeOSKeyStore.asyncDeleteSecret(
            BackupService.RECOVERY_OSKEYSTORE_LABEL
          );
        }
      }
    } finally {
      this.#recoveryInProgress = false;
    }
  }

  /**
   * Given a decompressed backup archive at recoveryPath, this method does the
   * following:
   *
   * 1. Reads in the backup manifest from the archive and ensures that it is
   *    valid.
   * 2. Creates a new named profile directory using the same name as the one
   *    found in the backup manifest, but with a different prefix.
   * 3. Iterates over each resource in the manifest and calls the recover()
   *    method on each found BackupResource, passing in the associated
   *    ManifestEntry from the backup manifest, and collects any post-recovery
   *    data from those resources.
   * 4. Writes a `post-recovery.json` file into the newly created profile
   *    directory.
   * 5. Returns the name of the newly created profile directory.
   * 6. Regardless of whether or not recovery succeeded, clears the native
   *    OSKeyStore of any secret labeled with
   *    BackupService.RECOVERY_OSKEYSTORE_LABEL.
   *
   * @param {string} recoveryPath
   *   The path to the decompressed backup archive on the file system.
   * @param {boolean} [shouldLaunch=false]
   *   An optional argument that specifies whether an instance of the app
   *   should be launched with the newly recovered profile after recovery is
   *   complete.
   * @param {string} [profileRootPath=null]
   *   An optional argument that specifies the root directory where the new
   *   profile directory should be created. If not provided, the default
   *   profile root directory will be used. This is primarily meant for
   *   testing.
   * @param {ArchiveEncryptionState} [encState=null]
   *   Set if the backup being recovered was encrypted. This implies that the
   *   profile being recovered was configured to create encrypted backups. This
   *   ArchiveEncryptionState is therefore needed to generate the
   *   ARCHIVE_ENCRYPTION_STATE_FILE for the recovered profile (since the
   *   original ARCHIVE_ENCRYPTION_STATE_FILE was intentionally not backed up,
   *   as the recovery device might have a different OSKeyStore secret).
   * @returns {Promise<nsIToolkitProfile>}
   *   The nsIToolkitProfile that was created for the recovered profile.
   * @throws {Exception}
   *   In the event that recovery somehow failed.
   */
  async recoverFromSnapshotFolder(
    recoveryPath,
    shouldLaunch = false,
    profileRootPath = null,
    encState = null
  ) {
    lazy.logConsole.debug("Recovering from backup at ", recoveryPath);

    try {
      // Read in the backup manifest.
      let manifestPath = PathUtils.join(
        recoveryPath,
        BackupService.MANIFEST_FILE_NAME
      );
      let manifest = await IOUtils.readJSON(manifestPath);
      if (!manifest.version) {
        throw new BackupError(
          "Backup manifest version not found",
          ERRORS.CORRUPTED_ARCHIVE
        );
      }

      if (manifest.version > lazy.ArchiveUtils.SCHEMA_VERSION) {
        throw new BackupError(
          "Cannot recover from a manifest newer than the current schema version",
          ERRORS.UNSUPPORTED_BACKUP_VERSION
        );
      }

      // Make sure that it conforms to the schema.
      let manifestSchema = await BackupService.getSchemaForVersion(
        SCHEMAS.BACKUP_MANIFEST,
        manifest.version
      );
      let schemaValidationResult = lazy.JsonSchema.validate(
        manifest,
        manifestSchema
      );
      if (!schemaValidationResult.valid) {
        lazy.logConsole.error(
          "Backup manifest does not conform to schema:",
          manifest,
          manifestSchema,
          schemaValidationResult
        );
        // TODO: Collect telemetry for this case. (bug 1891817)
        throw new BackupError(
          "Cannot recover from an invalid backup manifest",
          ERRORS.CORRUPTED_ARCHIVE
        );
      }

      // In the future, if we ever bump the ArchiveUtils.SCHEMA_VERSION and need
      // to do any special behaviours to interpret older schemas, this is where
      // we can do that, and we can remove this comment.

      let meta = manifest.meta;

      if (meta.appName != AppConstants.MOZ_APP_NAME) {
        throw new BackupError(
          `Cannot recover a backup from ${meta.appName} in ${AppConstants.MOZ_APP_NAME}`,
          ERRORS.UNSUPPORTED_APPLICATION
        );
      }

      if (
        Services.vc.compare(AppConstants.MOZ_APP_VERSION, meta.appVersion) < 0
      ) {
        throw new BackupError(
          `Cannot recover a backup created on version ${meta.appVersion} in ${AppConstants.MOZ_APP_VERSION}`,
          ERRORS.UNSUPPORTED_BACKUP_VERSION
        );
      }

      // Okay, we have a valid backup-manifest.json. Let's create a new profile
      // and start invoking the recover() method on each BackupResource.
      let profileSvc = Cc["@mozilla.org/toolkit/profile-service;1"].getService(
        Ci.nsIToolkitProfileService
      );
      let profile = profileSvc.createUniqueProfile(
        profileRootPath ? await IOUtils.getDirectory(profileRootPath) : null,
        meta.profileName
      );

      let postRecovery = {};

      // Iterate over each resource in the manifest and call recover() on each
      // associated BackupResource.
      for (let resourceKey in manifest.resources) {
        let manifestEntry = manifest.resources[resourceKey];
        let resourceClass = this.#resources.get(resourceKey);
        if (!resourceClass) {
          lazy.logConsole.error(
            `No BackupResource found for key ${resourceKey}`
          );
          continue;
        }

        try {
          lazy.logConsole.debug(
            `Restoring resource with key ${resourceKey}. ` +
              `Requires encryption: ${resourceClass.requiresEncryption}`
          );
          let resourcePath = PathUtils.join(recoveryPath, resourceKey);
          let postRecoveryEntry = await new resourceClass().recover(
            manifestEntry,
            resourcePath,
            profile.rootDir.path
          );
          postRecovery[resourceKey] = postRecoveryEntry;
        } catch (e) {
          lazy.logConsole.error(
            `Failed to recover resource: ${resourceKey}`,
            e
          );
        }
      }

      // Make sure that a legacy telemetry client ID exists and is written to
      // disk.
      let clientID = await lazy.ClientID.getClientID();
      lazy.logConsole.debug("Current client ID: ", clientID);
      // Next, copy over the legacy telemetry client ID state from the currently
      // running profile. The newly created profile that we're recovering into
      // should inherit this client ID.
      const TELEMETRY_STATE_FILENAME = "state.json";
      const TELEMETRY_STATE_FOLDER = "datareporting";
      await IOUtils.makeDirectory(
        PathUtils.join(profile.rootDir.path, TELEMETRY_STATE_FOLDER)
      );
      await IOUtils.copy(
        /* source */
        PathUtils.join(
          PathUtils.profileDir,
          TELEMETRY_STATE_FOLDER,
          TELEMETRY_STATE_FILENAME
        ),
        /* destination */
        PathUtils.join(
          profile.rootDir.path,
          TELEMETRY_STATE_FOLDER,
          TELEMETRY_STATE_FILENAME
        )
      );

      if (encState) {
        // The backup we're recovering was originally encrypted, meaning that
        // the recovered profile is configured to create encrypted backups. Our
        // caller passed us a _new_ ArchiveEncryptionState generated for this
        // device with the backup's recovery code so that we can serialize the
        // ArchiveEncryptionState for the recovered profile.
        let encStatePath = PathUtils.join(
          profile.rootDir.path,
          BackupService.PROFILE_FOLDER_NAME,
          BackupService.ARCHIVE_ENCRYPTION_STATE_FILE
        );
        let encStateObject = await encState.serialize();
        await IOUtils.writeJSON(encStatePath, encStateObject);
      }

      let postRecoveryPath = PathUtils.join(
        profile.rootDir.path,
        BackupService.POST_RECOVERY_FILE_NAME
      );
      await IOUtils.writeJSON(postRecoveryPath, postRecovery);

      profileSvc.flush();

      if (shouldLaunch) {
        Services.startup.createInstanceWithProfile(profile);
      }

      return profile;
    } catch (e) {
      lazy.logConsole.error(
        "Failed to recover from backup at ",
        recoveryPath,
        e
      );
      throw e;
    }
  }

  /**
   * Checks for the POST_RECOVERY_FILE_NAME in the current profile directory.
   * If one exists, instantiates any relevant BackupResource's, and calls
   * postRecovery() on them with the appropriate entry from the file. Once
   * this is done, deletes the file.
   *
   * The file is deleted even if one of the postRecovery() steps rejects or
   * fails.
   *
   * This function resolves silently if the POST_RECOVERY_FILE_NAME file does
   * not exist, which should be the majority of cases.
   *
   * @param {string} [profilePath=PathUtils.profileDir]
   *  The profile path to look for the POST_RECOVERY_FILE_NAME file. Defaults
   *  to the current profile.
   * @returns {Promise<undefined>}
   */
  async checkForPostRecovery(profilePath = PathUtils.profileDir) {
    lazy.logConsole.debug(`Checking for post-recovery file in ${profilePath}`);
    let postRecoveryFile = PathUtils.join(
      profilePath,
      BackupService.POST_RECOVERY_FILE_NAME
    );

    if (!(await IOUtils.exists(postRecoveryFile))) {
      lazy.logConsole.debug("Did not find post-recovery file.");
      this.#postRecoveryResolver();
      return;
    }

    lazy.logConsole.debug("Found post-recovery file. Loading...");

    try {
      let postRecovery = await IOUtils.readJSON(postRecoveryFile);
      for (let resourceKey in postRecovery) {
        let postRecoveryEntry = postRecovery[resourceKey];
        let resourceClass = this.#resources.get(resourceKey);
        if (!resourceClass) {
          lazy.logConsole.error(
            `Invalid resource for post-recovery step: ${resourceKey}`
          );
          continue;
        }

        lazy.logConsole.debug(`Running post-recovery step for ${resourceKey}`);
        await new resourceClass().postRecovery(postRecoveryEntry);
        lazy.logConsole.debug(`Done post-recovery step for ${resourceKey}`);
      }
    } finally {
      await IOUtils.remove(postRecoveryFile, { ignoreAbsent: true });
      this.#postRecoveryResolver();
    }
  }

  /**
   * Sets the parent directory of the backups folder. Calling this function will update
   * browser.backup.location.
   *
   * @param {string} parentDirPath directory path
   */
  setParentDirPath(parentDirPath) {
    try {
      if (!parentDirPath || !PathUtils.filename(parentDirPath)) {
        throw new BackupError(
          "Parent directory path is invalid.",
          ERRORS.FILE_SYSTEM_ERROR
        );
      }
      // Recreate the backups path with the new parent directory.
      let fullPath = PathUtils.join(
        parentDirPath,
        BackupService.BACKUP_DIR_NAME
      );
      Services.prefs.setStringPref(BACKUP_DIR_PREF_NAME, fullPath);
    } catch (e) {
      lazy.logConsole.error(
        `Failed to set parent directory ${parentDirPath}. ${e}`
      );
      throw e;
    }
  }

  /**
   * Updates backupDirPath in the backup service state. Should be called every time the value
   * for browser.backup.location changes.
   *
   * @param {string} newDirPath the new directory path for storing backups
   */
  async onUpdateLocationDirPath(newDirPath) {
    lazy.logConsole.debug(`Updating backup location to ${newDirPath}`);

    this.#_state.backupDirPath = newDirPath;
    this.stateUpdate();
  }

  /**
   * Returns the moz-icon URL of a file. To get the moz-icon URL, the
   * file path is convered to a fileURI. If there is a problem retreiving
   * the moz-icon due to an invalid file path, return null instead.
   *
   * @param {string} path Path of the file to read its icon from.
   * @returns {string|null} The moz-icon URL of the specified file, or
   *  null if the icon cannot be retreived.
   */
  getIconFromFilePath(path) {
    if (!path) {
      return null;
    }

    try {
      let fileURI = PathUtils.toFileURI(path);
      return `moz-icon:${fileURI}?size=16`;
    } catch (e) {
      return null;
    }
  }

  /**
   * Sets browser.backup.scheduled.enabled to true or false.
   *
   * @param { boolean } shouldEnableScheduledBackups true if scheduled backups should be enabled. Else, false.
   */
  setScheduledBackups(shouldEnableScheduledBackups) {
    Services.prefs.setBoolPref(
      SCHEDULED_BACKUPS_ENABLED_PREF_NAME,
      shouldEnableScheduledBackups
    );
  }

  /**
   * Updates scheduledBackupsEnabled in the backup service state. Should be called every time
   * the value for browser.backup.scheduled.enabled changes.
   *
   * @param {boolean} isScheduledBackupsEnabled True if scheduled backups are enabled. Else false.
   */
  onUpdateScheduledBackups(isScheduledBackupsEnabled) {
    if (this.#_state.scheduledBackupsEnabled != isScheduledBackupsEnabled) {
      lazy.logConsole.debug(
        "Updating scheduled backups",
        isScheduledBackupsEnabled
      );
      this.#_state.scheduledBackupsEnabled = isScheduledBackupsEnabled;
      this.stateUpdate();
    }
  }

  /**
   * Take measurements of the current profile state for Telemetry.
   *
   * @returns {Promise<undefined>}
   */
  async takeMeasurements() {
    lazy.logConsole.debug("Taking Telemetry measurements");

    // We'll start by measuring the available disk space on the storage
    // device that the profile directory is on.
    let profileDir = await IOUtils.getFile(PathUtils.profileDir);

    let profDDiskSpaceBytes = profileDir.diskSpaceAvailable;

    // Make the measurement fuzzier by rounding to the nearest 10MB.
    let profDDiskSpaceFuzzed = MeasurementUtils.fuzzByteSize(
      profDDiskSpaceBytes,
      10 * BYTES_IN_MEGABYTE
    );

    // And then record the value in kilobytes, since that's what everything
    // else is going to be measured in.
    Glean.browserBackup.profDDiskSpace.set(
      profDDiskSpaceFuzzed / BYTES_IN_KILOBYTE
    );

    // Measure the size of each file we are going to backup.
    for (let resourceClass of this.#resources.values()) {
      try {
        await new resourceClass().measure(PathUtils.profileDir);
      } catch (e) {
        lazy.logConsole.error(
          `Failed to measure for resource: ${resourceClass.key}`,
          e
        );
      }
    }
  }

  /**
   * The internal promise that is created on the first call to
   * loadEncryptionState.
   *
   * @type {Promise}
   */
  #loadEncryptionStatePromise = null;

  /**
   * Returns the current ArchiveEncryptionState. This method will only attempt
   * to read the state from the disk the first time it is called.
   *
   * @param {string} [profilePath=PathUtils.profileDir]
   *   The profile path where the encryption state might exist. This is only
   *   used for testing.
   * @returns {Promise<ArchiveEncryptionState>}
   */
  loadEncryptionState(profilePath = PathUtils.profileDir) {
    if (this.#encState !== undefined) {
      return Promise.resolve(this.#encState);
    }

    // This little dance makes it so that we only attempt to read the state off
    // of the disk the first time `loadEncryptionState` is called. Any
    // subsequent calls will await this same promise, OR, after the state has
    // been read in, they'll just get the #encState which is set after the
    // state has been read in.
    if (!this.#loadEncryptionStatePromise) {
      this.#loadEncryptionStatePromise = (async () => {
        // Default this to null here - that way, if we fail to read it in,
        // the null will indicate that we have at least _tried_ to load the
        // state.
        let encState = null;
        let encStateFile = PathUtils.join(
          profilePath,
          BackupService.PROFILE_FOLDER_NAME,
          BackupService.ARCHIVE_ENCRYPTION_STATE_FILE
        );

        // Try to read in any pre-existing encryption state. If that fails,
        // we fallback to not encrypting, and only backing up non-sensitive data.
        try {
          if (await IOUtils.exists(encStateFile)) {
            let stateObject = await IOUtils.readJSON(encStateFile);
            ({ instance: encState } =
              await lazy.ArchiveEncryptionState.initialize(stateObject));
          }
        } catch (e) {
          lazy.logConsole.error(
            "Failed to read / deserialize archive encryption state file: ",
            e
          );
          // TODO: This kind of error might be worth collecting telemetry on.
        }

        this.#_state.encryptionEnabled = !!encState;
        this.stateUpdate();

        this.#encState = encState;
        return encState;
      })();
    }

    return this.#loadEncryptionStatePromise;
  }

  /**
   * Enables encryption for backups, allowing sensitive data to be backed up.
   * Throws if encryption is already enabled. After enabling encryption, that
   * state is written to disk.
   *
   * @throws Exception
   * @param {string} password
   *   A non-blank password ("recovery code") that can be used to derive keys
   *   for encrypting the backup.
   * @param {string} [profilePath=PathUtils.profileDir]
   *   The profile path where the encryption state will be written. This is only
   *   used for testing.
   */
  async enableEncryption(password, profilePath = PathUtils.profileDir) {
    lazy.logConsole.debug("Enabling encryption.");
    let encState = await this.loadEncryptionState(profilePath);
    if (encState) {
      throw new BackupError(
        "Encryption is already enabled.",
        ERRORS.ENCRYPTION_ALREADY_ENABLED
      );
    }

    if (!password) {
      throw new BackupError(
        "Cannot supply a blank password.",
        ERRORS.INVALID_PASSWORD
      );
    }

    if (password.length < 8) {
      throw new BackupError(
        "Password must be at least 8 characters.",
        ERRORS.INVALID_PASSWORD
      );
    }

    // TODO: Enforce other password rules here, such as ensuring that the
    // password is not considered common.
    ({ instance: encState } = await lazy.ArchiveEncryptionState.initialize(
      password
    ));
    if (!encState) {
      throw new BackupError(
        "Failed to construct ArchiveEncryptionState",
        ERRORS.UNKNOWN
      );
    }

    this.#encState = encState;

    let encStateFile = PathUtils.join(
      profilePath,
      BackupService.PROFILE_FOLDER_NAME,
      BackupService.ARCHIVE_ENCRYPTION_STATE_FILE
    );

    let stateObj = await encState.serialize();
    await IOUtils.writeJSON(encStateFile, stateObj);

    this.#_state.encryptionEnabled = true;
    this.stateUpdate();
  }

  /**
   * Disables encryption of backups. Throws is encryption is already disabled.
   *
   * @throws Exception
   * @param {string} [profilePath=PathUtils.profileDir]
   *   The profile path where the encryption state exists. This is only used for
   *   testing.
   * @returns {Promise<undefined>}
   */
  async disableEncryption(profilePath = PathUtils.profileDir) {
    lazy.logConsole.debug("Disabling encryption.");
    let encState = await this.loadEncryptionState(profilePath);
    if (!encState) {
      throw new BackupError(
        "Encryption is already disabled.",
        ERRORS.ENCRYPTION_ALREADY_DISABLED
      );
    }

    let encStateFile = PathUtils.join(
      profilePath,
      BackupService.PROFILE_FOLDER_NAME,
      BackupService.ARCHIVE_ENCRYPTION_STATE_FILE
    );
    // It'd be pretty strange, but not impossible, for something else to have
    // gotten rid of the encryption state file at this point. We'll ignore it
    // if that's the case.
    await IOUtils.remove(encStateFile, { ignoreAbsent: true });

    this.#encState = null;
    this.#_state.encryptionEnabled = false;
    this.stateUpdate();
  }

  /**
   * The value of IDLE_THRESHOLD_SECONDS_PREF_NAME at the time that
   * initBackupScheduler was called. This is recorded so that if the preference
   * changes at runtime, that we properly remove the idle observer in
   * uninitBackupScheduler, since it's mapped to the idle time value.
   *
   * @see BackupService.initBackupScheduler()
   * @see BackupService.uninitBackupScheduler()
   * @type {number}
   */
  #idleThresholdSeconds = null;

  /**
   * An ES6 class that extends EventTarget cannot, apparently, be coerced into
   * a nsIObserver, even when we define QueryInterface. We work around this
   * limitation by having the observer be a function that we define at
   * registration time. We hold a reference to the observer so that we can
   * properly unregister.
   *
   * @see BackupService.initBackupScheduler()
   * @type {Function}
   */
  #observer = null;

  /**
   * True if the backup scheduler system has been initted via
   * initBackupScheduler().
   *
   * @see BackupService.initBackupScheduler()
   * @type {boolean}
   */
  #backupSchedulerInitted = false;

  /**
   * Initializes the backup scheduling system. This should be done shortly
   * after startup. It is exposed as a public method mainly for ease in testing.
   *
   * The scheduler will automatically uninitialize itself on the
   * quit-application-granted observer notification.
   *
   * @returns {Promise<undefined>}
   */
  async initBackupScheduler() {
    if (this.#backupSchedulerInitted) {
      lazy.logConsole.warn(
        "BackupService scheduler already initting or initted."
      );
      return;
    }

    this.#backupSchedulerInitted = true;

    let lastBackupPrefValue = Services.prefs.getIntPref(
      LAST_BACKUP_TIMESTAMP_PREF_NAME,
      0
    );
    if (!lastBackupPrefValue) {
      this.#_state.lastBackupDate = null;
    } else {
      this.#_state.lastBackupDate = lastBackupPrefValue;
    }

    this.#_state.lastBackupFileName = Services.prefs.getStringPref(
      LAST_BACKUP_FILE_NAME_PREF_NAME,
      ""
    );

    this.stateUpdate();

    // We'll default to 5 minutes of idle time unless otherwise configured.
    const FIVE_MINUTES_IN_SECONDS = 5 * 60;

    this.#idleThresholdSeconds = Services.prefs.getIntPref(
      IDLE_THRESHOLD_SECONDS_PREF_NAME,
      FIVE_MINUTES_IN_SECONDS
    );
    this.#observer = (subject, topic, data) => {
      this.onObserve(subject, topic, data);
    };
    lazy.logConsole.debug(
      `Registering idle observer for ${
        this.#idleThresholdSeconds
      } seconds of idle time`
    );
    lazy.idleService.addIdleObserver(
      this.#observer,
      this.#idleThresholdSeconds
    );
    lazy.logConsole.debug("Idle observer registered.");

    lazy.logConsole.debug(`Registering Places observer`);

    this.#placesObserver = new PlacesWeakCallbackWrapper(
      this.onPlacesEvents.bind(this)
    );
    PlacesObservers.addListener(
      ["history-cleared", "page-removed", "bookmark-removed"],
      this.#placesObserver
    );

    lazy.AddonManager.addAddonListener(this);

    Services.obs.addObserver(this.#observer, "passwordmgr-storage-changed");
    Services.obs.addObserver(this.#observer, "formautofill-storage-changed");
    Services.obs.addObserver(this.#observer, "sanitizer-sanitization-complete");
    Services.obs.addObserver(this.#observer, "perm-changed");
    Services.obs.addObserver(this.#observer, "quit-application-granted");
  }

  /**
   * Uninitializes the backup scheduling system.
   *
   * @returns {Promise<undefined>}
   */
  async uninitBackupScheduler() {
    if (!this.#backupSchedulerInitted) {
      lazy.logConsole.warn(
        "Tried to uninitBackupScheduler when it wasn't yet enabled."
      );
      return;
    }

    lazy.idleService.removeIdleObserver(
      this.#observer,
      this.#idleThresholdSeconds
    );

    PlacesObservers.removeListener(
      ["history-cleared", "page-removed", "bookmark-removed"],
      this.#placesObserver
    );

    lazy.AddonManager.removeAddonListener(this);

    Services.obs.removeObserver(this.#observer, "passwordmgr-storage-changed");
    Services.obs.removeObserver(this.#observer, "formautofill-storage-changed");
    Services.obs.removeObserver(
      this.#observer,
      "sanitizer-sanitization-complete"
    );
    Services.obs.removeObserver(this.#observer, "perm-changed");
    Services.obs.removeObserver(this.#observer, "quit-application-granted");
    this.#observer = null;

    this.#regenerationDebouncer.disarm();
    this.#backupWriteAbortController.abort();
  }

  /**
   * Called by this.#observer on idle from the nsIUserIdleService or
   * quit-application-granted from the nsIObserverService. Exposed as a public
   * method mainly for ease in testing.
   *
   * @param {nsISupports|null} subject
   *   The nsIUserIdleService for the idle notification, and null for the
   *   quit-application-granted topic.
   * @param {string} topic
   *   The topic that the notification belongs to.
   * @param {string} data
   *   Optional data that was included with the notification.
   */
  onObserve(subject, topic, data) {
    switch (topic) {
      case "idle": {
        this.onIdle();
        break;
      }
      case "quit-application-granted": {
        this.uninitBackupScheduler();
        break;
      }
      case "passwordmgr-storage-changed": {
        if (data == "removeLogin" || data == "removeAllLogins") {
          this.#debounceRegeneration();
        }
        break;
      }
      case "formautofill-storage-changed": {
        if (
          data == "remove" &&
          (subject.wrappedJSObject.collectionName == "creditCards" ||
            subject.wrappedJSObject.collectionName == "addresses")
        ) {
          this.#debounceRegeneration();
        }
        break;
      }
      case "sanitizer-sanitization-complete": {
        this.#debounceRegeneration();
        break;
      }
      case "perm-changed": {
        if (data == "deleted") {
          this.#debounceRegeneration();
        }
        break;
      }
    }
  }

  /**
   * Called when the last known backup should be deleted and a new one
   * created. This uses the #regenerationDebouncer to debounce clusters of
   * events that might cause such a regeneration to occur.
   */
  #debounceRegeneration() {
    this.#regenerationDebouncer.disarm();
    this.#regenerationDebouncer.arm();
  }

  /**
   * Called when the nsIUserIdleService reports that user input events have
   * not been sent to the application for at least
   * IDLE_THRESHOLD_SECONDS_PREF_NAME seconds.
   */
  onIdle() {
    lazy.logConsole.debug("Saw idle callback");
    if (lazy.scheduledBackupsPref) {
      lazy.logConsole.debug("Scheduled backups enabled.");
      let now = Math.floor(Date.now() / 1000);
      let lastBackupDate = this.#_state.lastBackupDate;
      if (lastBackupDate && lastBackupDate > now) {
        lazy.logConsole.error(
          "Last backup was somehow in the future. Resetting the preference."
        );
        lastBackupDate = null;
        this.#_state.lastBackupDate = null;
        this.stateUpdate();
      }

      if (!lastBackupDate) {
        lazy.logConsole.debug("No last backup time recorded in prefs.");
      } else {
        lazy.logConsole.debug(
          "Last backup was: ",
          new Date(lastBackupDate * 1000)
        );
      }

      if (
        !lastBackupDate ||
        now - lastBackupDate > lazy.minimumTimeBetweenBackupsSeconds
      ) {
        lazy.logConsole.debug(
          "Last backup exceeded minimum time between backups. Queing a " +
            "backup via idleDispatch."
        );
        // Just because the user hasn't sent us events in a while doesn't mean
        // that the browser itself isn't busy. It might be, for example, playing
        // video or doing a complex calculation that the user is actively
        // waiting to complete, and we don't want to draw resources from that.
        // Instead, we'll use ChromeUtils.idleDispatch to wait until the event
        // loop in the parent process isn't so busy with higher priority things.
        this.createBackupOnIdleDispatch();
      } else {
        lazy.logConsole.debug(
          "Last backup was too recent. Not creating one for now."
        );
      }
    }
  }

  /**
   * Calls BackupService.createBackup at the next moment when the event queue
   * is not busy with higher priority events. This is intentionally broken out
   * into its own method to make it easier to stub out in tests.
   */
  createBackupOnIdleDispatch() {
    ChromeUtils.idleDispatch(() => {
      lazy.logConsole.debug(
        "idleDispatch fired. Attempting to create a backup."
      );
      this.createBackup();
    });
  }

  /**
   * Handler for events coming in through our PlacesObserver.
   *
   * @param {PlacesEvent[]} placesEvents
   *   One or more batched events that are of a type that we subscribed to.
   */
  onPlacesEvents(placesEvents) {
    // Note that if any of the events that we iterate result in a regeneration
    // being queued, we simply return without the processing the rest, as there
    // is not really a point.
    for (let event of placesEvents) {
      switch (event.type) {
        case "page-removed": {
          // We will get a page-removed event if a page has been deleted both
          // manually by a user, but also automatically if the page has "aged
          // out" of the Places database. We only want to regenerate backups
          // in the manual case (REASON_DELETED).
          if (event.reason == PlacesVisitRemoved.REASON_DELETED) {
            this.#debounceRegeneration();
            return;
          }
          break;
        }
        case "bookmark-removed":
        // Intentional fall-through
        case "history-cleared": {
          this.#debounceRegeneration();
          return;
        }
      }
    }
  }

  /**
   * This method is the only method of the AddonListener interface that
   * BackupService implements and is called by AddonManager when an addon
   * is uninstalled.
   *
   * @param {AddonInternal} _addon
   *   The addon being uninstalled.
   */
  onUninstalled(_addon) {
    this.#debounceRegeneration();
  }

  /**
   * Gets a sample from a given backup file and sets a subset of that as
   * the backupFileInfo in the backup service state.
   *
   * Called when getting a info for an archive to potentially restore.
   *
   * @param {string} backupFilePath path to the backup file to sample.
   */
  async getBackupFileInfo(backupFilePath) {
    lazy.logConsole.debug(`Getting info from backup file at ${backupFilePath}`);
    let { archiveJSON, isEncrypted } = await this.sampleArchive(backupFilePath);
    this.#_state.backupFileInfo = {
      isEncrypted,
      date: archiveJSON?.meta?.date,
    };
    this.stateUpdate();
  }

  /*
   * Attempts to open a native file explorer window at the last backup file's
   * location on the filesystem.
   */
  async showBackupLocation() {
    let backupFilePath = PathUtils.join(
      lazy.backupDirPref,
      this.#_state.lastBackupFileName
    );
    if (await IOUtils.exists(backupFilePath)) {
      new lazy.nsLocalFile(backupFilePath).reveal();
    } else {
      let archiveDestFolderPath = await this.resolveArchiveDestFolderPath(
        lazy.backupDirPref
      );
      new lazy.nsLocalFile(archiveDestFolderPath).reveal();
    }
  }

  /**
   * Shows a native folder picker to set the location to write the single-file
   * archive files.
   *
   * @param {ChromeWindow} window
   *   The top-level browsing window to associate the file picker with.
   * @returns {Promise<undefined>}
   */
  async editBackupLocation(window) {
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    let mode = Ci.nsIFilePicker.modeGetFolder;
    fp.init(window.browsingContext, "", mode);

    let currentBackupDirPathParent = PathUtils.parent(
      this.#_state.backupDirPath
    );
    if (await IOUtils.exists(currentBackupDirPathParent)) {
      fp.displayDirectory = await IOUtils.getDirectory(
        currentBackupDirPathParent
      );
    }

    let result = await new Promise(resolve => fp.open(resolve));

    if (result === Ci.nsIFilePicker.returnCancel) {
      return;
    }

    let path = fp.file.path;

    // If the same parent directory was chosen, this is a no-op.
    if (
      PathUtils.join(path, BackupService.BACKUP_DIR_NAME) == lazy.backupDirPref
    ) {
      return;
    }

    // If the location changed, delete the last backup there if one exists.
    await this.deleteLastBackup();
    this.setParentDirPath(path);
  }

  /**
   * Will attempt to delete the last created single-file archive if it exists.
   * Once done, this method will also check the parent folder to see if it's
   * empty. If so, then the folder is removed.
   *
   * @returns {Promise<undefined>}
   */
  async deleteLastBackup() {
    return locks.request(
      BackupService.WRITE_BACKUP_LOCK_NAME,
      { signal: this.#backupWriteAbortController.signal },
      async () => {
        if (this.#_state.lastBackupFileName) {
          let backupFilePath = PathUtils.join(
            lazy.backupDirPref,
            this.#_state.lastBackupFileName
          );

          lazy.logConsole.log(
            "Attempting to delete last backup file at ",
            backupFilePath
          );
          await IOUtils.remove(backupFilePath, { ignoreAbsent: true });

          this.#_state.lastBackupDate = null;
          Services.prefs.clearUserPref(LAST_BACKUP_TIMESTAMP_PREF_NAME);

          this.#_state.lastBackupFileName = "";
          Services.prefs.clearUserPref(LAST_BACKUP_FILE_NAME_PREF_NAME);

          this.stateUpdate();
        } else {
          lazy.logConsole.log(
            "Not deleting last backup file, since none is known about."
          );
        }

        if (await IOUtils.exists(lazy.backupDirPref)) {
          // See if there are any other files lingering around in the destination
          // folder. If not, delete that folder too.
          let children = await IOUtils.getChildren(lazy.backupDirPref);
          if (!children.length) {
            await IOUtils.remove(lazy.backupDirPref);
          }
        }
      }
    );
  }
}
