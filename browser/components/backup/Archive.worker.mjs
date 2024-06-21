/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { PromiseWorker } from "resource://gre/modules/workers/PromiseWorker.mjs";

// The ArchiveUtils module is designed to be imported in both worker and
// main thread contexts.
/* eslint-disable mozilla/reject-import-system-module-from-non-system */
import { ArchiveUtils } from "resource:///modules/backup/ArchiveUtils.sys.mjs";
import { ArchiveEncryptor } from "resource:///modules/backup/ArchiveEncryption.sys.mjs";

/**
 * An ArchiveWorker is a PromiseWorker that tries to do most of the heavy
 * lifting of dealing with single-file archives for backups, to avoid doing
 * much on the main thread. This is mostly important for single-file archive
 * _creation_, as this is supposed to occur silently in the background without
 * the user noticing any degredation in performance.
 */
class ArchiveWorker {
  #worker = null;

  constructor() {
    // Connect the provider to the worker.
    this.#connectToPromiseWorker();
  }

  /**
   * Generates a boundary string that can be used to separate sections in a
   * multipart/mixed MIME message.
   *
   * See https://www.w3.org/Protocols/rfc1341/7_2_Multipart.html.
   *
   * @returns {string}
   */
  #generateBoundary() {
    return (
      "----=_Part_" +
      new Date().getTime() +
      "_" +
      Math.random().toString(36).slice(2, 12) +
      "_" +
      Math.random().toString(36).slice(2, 12)
    );
  }

  /**
   * Calculates how many base64 bytes will be generated from some number of
   * unencoded bytes. This presumes that the base64 bytes include a newline
   * terminator at the end.
   *
   * @param {number} bytes
   *   The number of bytes to be converted to base64.
   * @param {boolean} encrypting
   *   True if encryption via ArchiveEncryptor is being applied.
   * @returns {number}
   */
  #computeChunkBase64Bytes(bytes, encrypting) {
    if (encrypting) {
      bytes += ArchiveUtils.TAG_LENGTH_BYTES;
    }

    return 4 * Math.ceil(bytes / 3) + 1;
  }

  /**
   * @typedef {object} EncryptionArgs
   * @property {CryptoKey} publicKey
   *   The RSA-OAEP public key that will be used to derive keys for encrypting
   *   the backup.
   * @property {CryptoKey} backupAuthKey
   *   The AES-GCM key that will be used to authenticate the owner of the
   *   backup.
   * @property {Uint8Array} wrappedSecrets
   *   The encrypted backup secrets computed by ArchiveEncryptionState.
   * @property {Uint8Array} salt
   *   A salt computed for the PBKDF2 stretching of the recovery code.
   * @property {Uint8Array} nonce
   *   A nonce computed when wrapping the private key and OSKeyStore secret.
   */

  /**
   * Constructs a single-file archive for a backup on the filesystem. A
   * single-file archive is a specially crafted HTML document that includes,
   * among other things, an inlined multipart/mixed MIME message within a
   * document comment.
   *
   * @param {object} params
   *   Arguments that are described in more detail below.
   * @param {string} params.archivePath
   *   The path on the file system to write the single-file archive.
   * @param {string} params.templateURI
   *   A URI pointing to the HTML template that will be used for the viewable
   *   part of the document. The inlined MIME message will be appended after
   *   the contents of this template.
   * @param {object} params.backupMetadata
   *   The metadata associated with this backup. This is a copy of the metadata
   *   object that is contained within the compressed backups' manifest.
   * @param {string} params.compressedBackupSnapshotPath
   *   The path on the file system where the compressed backup file is located.
   * @param {EncryptionArgs} [params.encryptionArgs=undefined]
   *   Optional EncryptionArgs, which will be used to encrypt this archive.
   * @returns {Promise<undefined>}
   */
  async constructArchive({
    archivePath,
    templateURI,
    backupMetadata,
    compressedBackupSnapshotPath,
    encryptionArgs,
  }) {
    let encryptor = null;
    if (encryptionArgs) {
      encryptor = await ArchiveEncryptor.initialize(
        encryptionArgs.publicKey,
        encryptionArgs.backupAuthKey
      );
    }

    // We can get at the template content by using a sync XHR, which is fine to
    // to do in a Worker.
    let templateXhr = new XMLHttpRequest();
    // Using a synchronous XHR in a worker is fine.
    templateXhr.open("GET", templateURI, false);
    templateXhr.responseType = "text";
    templateXhr.send(null);
    let template = templateXhr.responseText;

    let boundary = this.#generateBoundary();

    let jsonBlock;
    if (encryptor) {
      jsonBlock = await encryptor.confirm(
        backupMetadata,
        encryptionArgs.wrappedSecrets,
        encryptionArgs.salt,
        encryptionArgs.nonce
      );
    } else {
      jsonBlock = {
        version: ArchiveUtils.SCHEMA_VERSION,
        encConfig: null,
        meta: backupMetadata,
      };
    }

    let serializedJsonBlock = JSON.stringify(jsonBlock);
    let textEncoder = new TextEncoder();
    let jsonBlockLength = textEncoder.encode(serializedJsonBlock).length;

    // Once we get the ability to stream to the filesystem from IOUtils in a
    // worker, we should use that instead of appending each of these chunks.
    //
    // This isn't supposed to be some kind of generalized MIME message
    // generator, so we're happy to construct it by hand here.
    await IOUtils.writeUTF8(archivePath, template);
    await IOUtils.writeUTF8(
      archivePath,
      `
${ArchiveUtils.INLINE_MIME_START_MARKER}
Content-Type: multipart/mixed; boundary="${boundary}"

--${boundary}
Content-Type: application/json; charset=utf-8
Content-Disposition: attachment; filename="archive.json"
Content-Length: ${jsonBlockLength}

${JSON.stringify(jsonBlock)}
`,
      { mode: "append" }
    );

    let compressedBackupSnapshotFile = IOUtils.openFileForSyncReading(
      compressedBackupSnapshotPath
    );
    let totalBytesToRead = compressedBackupSnapshotFile.size;

    // To calculate the Content-Length of the base64 block, we start by
    // computing how many newlines we'll be adding...
    let totalNewlines = Math.ceil(
      totalBytesToRead / ArchiveUtils.ARCHIVE_CHUNK_MAX_BYTES_SIZE
    );

    // Next, we determine how many full-sized chunks of
    // ARCHIVE_CHUNK_MAX_BYTES_SIZE we'll be using, and multiply that by the
    // number of base64 bytes that such a chunk will require.
    let fullSizeChunks = totalNewlines - 1;
    let fullSizeChunkBase64Bytes = this.#computeChunkBase64Bytes(
      ArchiveUtils.ARCHIVE_CHUNK_MAX_BYTES_SIZE,
      !!encryptor
    );
    let totalBase64Bytes = fullSizeChunks * fullSizeChunkBase64Bytes;

    // Finally, if there are any leftover bytes that are less than
    // ARCHIVE_CHUNK_MAX_BYTES_SIZE, determine how many bytes those will
    // require, and add it to our total.
    let leftoverChunkBytes =
      totalBytesToRead % ArchiveUtils.ARCHIVE_CHUNK_MAX_BYTES_SIZE;
    if (leftoverChunkBytes) {
      totalBase64Bytes += this.#computeChunkBase64Bytes(
        leftoverChunkBytes,
        !!encryptor
      );
    }

    await IOUtils.writeUTF8(
      archivePath,
      `--${boundary}
Content-Type: application/octet-stream
Content-Disposition: attachment; filename="archive.zip"
Content-Transfer-Encoding: base64
Content-Length: ${totalBase64Bytes}

`,
      { mode: "append" }
    );

    // And now we read in the bytes of the compressed file, base64 encode them,
    // and append them to the document. Down the line, this is also where
    // encryption will be done.
    let currentIndex = 0;
    while (currentIndex < totalBytesToRead) {
      let bytesToRead = Math.min(
        ArchiveUtils.ARCHIVE_CHUNK_MAX_BYTES_SIZE,
        totalBytesToRead - currentIndex
      );
      if (bytesToRead <= 0) {
        throw new Error(
          "Failed to calculate the right number of bytes to read."
        );
      }

      let buffer = new Uint8Array(bytesToRead);
      compressedBackupSnapshotFile.readBytesInto(buffer, currentIndex);

      let bytesToWrite;

      if (encryptor) {
        let isLastChunk =
          bytesToRead < ArchiveUtils.ARCHIVE_CHUNK_MAX_BYTES_SIZE;
        bytesToWrite = await encryptor.encrypt(buffer, isLastChunk);
      } else {
        bytesToWrite = buffer;
      }

      // We're very intentionally newline-separating these blocks here, as
      // these blocks may have been run through encryption, and the same blocks
      // must be run through decryption to unpack the archive.
      // Newline-separation makes it easier to identify and manage these blocks.
      await IOUtils.writeUTF8(
        archivePath,
        ArchiveUtils.arrayToBase64(bytesToWrite) + "\n",
        {
          mode: "append",
        }
      );

      currentIndex += bytesToRead;
    }

    await IOUtils.writeUTF8(
      archivePath,
      `
--${boundary}
${ArchiveUtils.INLINE_MIME_END_MARKER}
`,
      { mode: "append" }
    );

    compressedBackupSnapshotFile.close();

    return true;
  }

  /**
   * @typedef {object} ArchiveHeaderResult
   * @property {string} contentType
   *   The value of the Content-Type for the inlined MIME message.
   * @property {number} startByteOffset
   *   The byte offset within the archive file where the inlined MIME message
   *   begins.
   */

  /**
   * Given a path to a single-file archive HTML file, this method will sniff
   * the header of the file to make sure it matches one that we support. If
   * successful, it will resolve with the contentType of the inline MIME
   * message, as well as the byte offset for which the start of the inlined MIME
   * message can be read from.
   *
   * @param {string} archivePath
   *   The path to a single-file archive HTML file.
   * @returns {Promise<ArchiveHeaderResult, Error>}
   */
  parseArchiveHeader(archivePath) {
    // We expect the first bytes of the file to indicate that this is an HTML5
    // file and to give us a version number we can handle.
    let syncReadFile = IOUtils.openFileForSyncReading(archivePath);
    let totalBytes = syncReadFile.size;

    // This seems like a reasonable minimum number of bytes to read in to get
    // at the header. If the header data isn't in there, then it's a corrupt
    // file.
    const MAX_BYTES_TO_READ = 256;
    let headerBytesToRead = Math.min(
      MAX_BYTES_TO_READ,
      totalBytes - MAX_BYTES_TO_READ
    );
    let headerBuffer = new Uint8Array(headerBytesToRead);
    syncReadFile.readBytesInto(headerBuffer, 0);

    let textDecoder = new TextDecoder();
    let decodedHeader = textDecoder.decode(headerBuffer);
    const EXPECTED_HEADER = /^<!DOCTYPE html>\n<!-- Version: (\d+) -->\n/;
    let headerMatches = decodedHeader.match(EXPECTED_HEADER);
    if (!headerMatches) {
      throw new Error("Corrupt archive header");
    }

    let version = parseInt(headerMatches[1], 10);
    // In the future, if we ever bump the ARCHIVE_FILE_VERSION, this is where we
    // could place migrations / handlers for older archive versions.
    if (version != ArchiveUtils.ARCHIVE_FILE_VERSION) {
      throw new Error("Unsupported archive version: " + version);
    }

    // Now we have to scan forward, looking for the INLINE_MIME_MARKER_START
    // and the Content-Type, which appears just before the MIME message.
    //
    // We scan by reading bytes into a buffer rather than reading in the whole
    // file, since the file could be quite large (100s of MB).
    let currentIndex = headerBuffer.byteLength;

    let startByteOffset = 0;
    // We keep the old buffer around, and always join it with the buffer that
    // contains the recently read-in bytes. That way, we can account for the
    // possibility that the INLINE_MIME_START_MARKER and Content-Type were
    // only half-loaded in prior or current buffer.
    let oldBuffer = headerBuffer;
    let priorIndex = 0;
    let contentType = null;
    const EXPECTED_MARKER = new RegExp(
      `${ArchiveUtils.INLINE_MIME_START_MARKER}\nContent-Type: (.+)\n\n`
    );

    let textEncoder = new TextEncoder();
    while (currentIndex < totalBytes) {
      let bytesToRead = Math.min(MAX_BYTES_TO_READ, totalBytes - currentIndex);

      // This shouldn't happen, but better safe than sorry.
      if (bytesToRead <= 0) {
        throw new Error(
          "Failed to calculate the proper number of bytes to read: " +
            bytesToRead
        );
      }

      let buffer = new Uint8Array(bytesToRead);
      syncReadFile.readBytesInto(buffer, currentIndex);

      let combinedBuffer = new Uint8Array(
        oldBuffer.byteLength + buffer.byteLength
      );
      combinedBuffer.set(oldBuffer, 0);
      combinedBuffer.set(buffer, oldBuffer.byteLength);

      // Now we look for the inline MIME marker, and try to extract the
      // Content-Type for it.
      let decodedString = textDecoder.decode(combinedBuffer);
      let markerMatches = decodedString.match(EXPECTED_MARKER);

      if (markerMatches) {
        // If we found it, we want to find the byte index for the point
        // immediately after the match. You'd think we could use
        // decodedString.search for this, but unfortunately search returns
        // character indexes and not byte indexes (and Unicode characters,
        // which might be displayed in the markup of the page, are multiple
        // bytes long). To work around this, we use a TextEncoder to encode
        // everything leading up to the marker, and count the number of bytes.
        // Then we count the number of bytes in our match. The sum of these
        // two values, plus the priorIndex gives us the byte index of the point
        // right after our regular expression match in a Unicode-character
        // compatible way.
        //
        // This all presumes that the archive file was encoded as UTF-8. Since
        // we control the generation of this file, this is a safe assumption.
        let match = markerMatches[0];
        let matchIndex = decodedString.indexOf(match);
        let substringUpToMatch = decodedString.slice(0, matchIndex);
        let substringUpToMatchBytes =
          textEncoder.encode(substringUpToMatch).byteLength;
        let matchBytes = textEncoder.encode(markerMatches[0]).byteLength;
        startByteOffset = priorIndex + substringUpToMatchBytes + matchBytes;
        contentType = markerMatches[1];
        break;
      }

      priorIndex = currentIndex;
      currentIndex += bytesToRead;
      oldBuffer = buffer;
    }
    return { startByteOffset, contentType };
  }

  /**
   * Implements the standard boilerplate to make this class work as a
   * PromiseWorker.
   */
  #connectToPromiseWorker() {
    this.#worker = new PromiseWorker.AbstractWorker();
    this.#worker.dispatch = (method, args = []) => {
      if (!this[method]) {
        throw new Error("Method does not exist: " + method);
      }
      return this[method](...args);
    };
    this.#worker.close = () => self.close();
    this.#worker.postMessage = (message, ...transfers) => {
      self.postMessage(message, ...transfers);
    };

    self.callMainThread = this.#worker.callMainThread.bind(this.#worker);
    self.addEventListener("message", msg => this.#worker.handleMessage(msg));
    self.addEventListener("unhandledrejection", function (error) {
      throw error.reason;
    });
  }
}

new ArchiveWorker();
