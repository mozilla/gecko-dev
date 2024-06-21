/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This module expects to be able to load in both main-thread module contexts,
// as well as ChromeWorker contexts. Do not ChromeUtils.importESModule
// anything there at the top-level that's not compatible with both contexts.

// The ArchiveUtils module is designed to be imported in both worker and
// main thread contexts.
import { ArchiveUtils } from "resource:///modules/backup/ArchiveUtils.sys.mjs";

/**
 * Both ArchiveEncryptor and ArchiveDecryptor maintain an internal nonce used as
 * a big-endian chunk counter. That counter is Uint8Array(16) array, which makes
 * doing simple things like adding to the counter somewhat cumbersome.
 * NonceUtils contains helper methods to do nonce-related management and
 * arithmetic.
 */
export const NonceUtils = {
  /**
   * Flips the bit in the nonce to indicate that the nonce will be used for the
   * last chunk to be encrypted. The specification calls for this bit to be the
   * 12th bit from the end.
   *
   * @param {Uint8Array} nonce
   *   The nonce to flip the bit on.
   */
  setLastChunkOnNonce(nonce) {
    if (nonce[4] != 0) {
      throw new Error("Last chunk byte on nonce already set!");
    }

    // The nonce is 16 bytes so that we can use DataView / getBigUint64 for
    // arithmetic, but the spec says that we set the top byte of a 12-byte nonce
    // to 0x01. We ignore the first 4 bytes of the 16-byte nonce then, and stick
    // the 1 on the 12th byte (which in big-endian order is the 4th byte).
    nonce[4] = 1;
  },

  /**
   * Returns true if `setLastChunkOnNonce` has been called on the nonce already.
   *
   * @param {Uint8Array} nonce
   *   The nonce to check for the bit on.
   * @returns {boolean}
   */
  lastChunkSetOnNonce(nonce) {
    return nonce[4] == 1;
  },

  /**
   * Increments a nonce by some amount (defaulting to 1). The nonce should be
   * incremented once per chunk of maximum ARCHIVE_CHUNK_MAX_BYTES_SIZE bytes.
   * If this incrementing indicates that the number of bytes encrypted exceeds
   * ARCHIVE_MAX_BYTES_SIZE, an exception is thrown.
   *
   * @param {Uint8Array} nonce
   *   The nonce to increment.
   * @param {number} [incrementBy=1]
   *   The amount to increment the nonce by, defaulting to 1.
   */
  incrementNonce(nonce, incrementBy = 1) {
    let view = new DataView(nonce.buffer, 8);
    let nonceBigInt = view.getBigUint64(0);
    nonceBigInt += BigInt(incrementBy);
    if (
      nonceBigInt * BigInt(ArchiveUtils.ARCHIVE_CHUNK_MAX_BYTES_SIZE) >
      BigInt(ArchiveUtils.ARCHIVE_MAX_BYTES_SIZE)
    ) {
      throw new Error("Exceeded archive maximum size.");
    }

    view.setBigUint64(0, nonceBigInt);
  },
};

/**
 * A class that is used to encrypt one or more chunks of a backup archive.
 * Callers must use the async static initialize() method to create an
 * ArchiveEncryptor, and then can encrypt() individual chunks. Callers can
 * call confirm() to generate the serializable JSON block to be included with
 * the archive.
 */
export class ArchiveEncryptor {
  /**
   * A hack that lets us ensure that an ArchiveEncryptor cannot be
   * constructed except via the ArchiveEncryptor.initialize static
   * method.
   *
   * See https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Classes/Private_properties#simulating_private_constructors
   */
  static #isInternalConstructing = false;

  /**
   * The RSA-OAEP public key generated via an ArchiveEncryptionState to
   * encrypt a backup.
   *
   * @type {CryptoKey}
   */
  #publicKey = null;

  /**
   * A unique key generated for the individual archive, used to MAC the
   * metadata for a backup.
   *
   * @type {CryptoKey}
   */
  #authKey = null;

  /**
   * The wrapped archive encryption key material. The archive encryption key
   * material is randomly generated per backup to derive the encryption keys
   * for encrypting the backup, and is then wrapped using the #publicKey.
   *
   * @type {Uint8Array}
   */
  #wrappedArchiveKeyMaterial = null;

  /**
   * The derived AES-GCM encryption key used to encrypt chunks of the archive.
   *
   * @type {CryptoKey}
   */
  #encKey = null;

  /**
   * A big-endian counter nonce, incremented for each subsequent chunk of the
   * encrypted archive. The size of the nonce must be a multiple of 8 in order
   * to simplify the arithmetic via DataView / getBigUint64 / setBigUint64.
   *
   * @type {Uint8Array}
   */
  #nonce = new Uint8Array(16);

  /**
   * @see ArchiveEncryptor.#isInternalConstructing
   */
  constructor() {
    if (!ArchiveEncryptor.#isInternalConstructing) {
      throw new Error("ArchiveEncryptor is not constructable.");
    }
    ArchiveEncryptor.#isInternalConstructing = false;
  }

  /**
   * True if the last chunk flag has been set on the nonce already. Once this
   * returns true, no further chunks can be encrypted.
   *
   * @returns {boolean}
   */
  #isDone() {
    return NonceUtils.lastChunkSetOnNonce(this.#nonce);
  }

  /**
   * Constructs an ArchiveEncryptor to prepare it to encrypt chunks of an
   * archive. This must only be called via the ArchiveEncryptor.initialize
   * static method.
   *
   * @param {CryptoKey} publicKey
   *   The RSA-OAEP public key generated by an ArchiveEncryptionState.
   * @param {CryptoKey} backupAuthKey
   *   The AES-GCM BackupAuthKey generated by an ArchiveEncryptionState.
   * @returns {Promise<undefined>}
   */
  async #initialize(publicKey, backupAuthKey) {
    this.#publicKey = publicKey;

    // Generate a random archive key ArchiveKey. The key material is 256 random
    // bits.
    let archiveKeyMaterial = crypto.getRandomValues(new Uint8Array(32));

    // Encrypt ArchiveKey with the RSA-OEAP Public Key to form WrappedArchiveKey
    this.#wrappedArchiveKeyMaterial = new Uint8Array(
      await crypto.subtle.encrypt(
        {
          name: "RSA-OAEP",
        },
        this.#publicKey,
        archiveKeyMaterial
      )
    );

    let { archiveEncKey, authKey } = await ArchiveUtils.computeEncryptionKeys(
      archiveKeyMaterial,
      backupAuthKey
    );
    this.#authKey = authKey;
    this.#encKey = archiveEncKey;
  }

  /**
   * Encrypts a chunk from a backup archive.
   *
   * @param {Uint8Array} plaintextChunk
   *   The plaintext chunk of bytes to encrypt.
   * @param {boolean} [isLastChunk=false]
   *   Callers should set this to true if the chunk being encrypted is the
   *   last chunk. Once this is done, no additional chunk can be encrypted.
   * @returns {Promise<Uint8Array>}
   */
  async encrypt(plaintextChunk, isLastChunk = false) {
    if (this.#isDone()) {
      throw new Error(
        "Cannot encrypt any more chunks with this ArchiveEncryptor."
      );
    }

    if (plaintextChunk.byteLength > ArchiveUtils.ARCHIVE_CHUNK_MAX_BYTES_SIZE) {
      throw new Error(
        `Chunk is too large to encrypt: ${plaintextChunk.byteLength} bytes`
      );
    }
    if (
      plaintextChunk.byteLength != ArchiveUtils.ARCHIVE_CHUNK_MAX_BYTES_SIZE &&
      !isLastChunk
    ) {
      throw new Error("Only last chunk can be smaller than the chunk max size");
    }

    if (isLastChunk) {
      NonceUtils.setLastChunkOnNonce(this.#nonce);
    }

    let ciphertextChunk;
    try {
      ciphertextChunk = await crypto.subtle.encrypt(
        {
          name: "AES-GCM",
          /**
           * Take only the last 12 bytes of the nonce, since the WebCrypto API
           * starts to behave differently when the IV is > 96 bits.
           */
          iv: this.#nonce.subarray(4),
          tagLength: ArchiveUtils.TAG_LENGTH,
        },
        this.#encKey,
        plaintextChunk
      );
    } catch (e) {
      throw new Error("Failed to encrypt a chunk.");
    }

    NonceUtils.incrementNonce(this.#nonce);

    return new Uint8Array(ciphertextChunk);
  }

  /**
   * Signs the metadata of a backup archive. This signature is used to both
   * provide an easy way of checking that a recovery code is valid, but also to
   * ensure that the metadata has not been tampered with. The returned Promise
   * resolves with the JSON block that can be written to the backup archive
   * file.
   *
   * @param {object} meta
   *   The metadata of a backup archive.
   * @param {Uint8Array} wrappedSecrets
   *   The encrypted backup secrets computed by ArchiveEncryptionState.
   * @param {Uint8Array} salt
   *   The salt used by ArchiveEncryptionState for the PBKDF2 stretching of the
   *   recovery code.
   * @param {Uint8Array} nonce
   *   The nonce used by ArchiveEncryptionState when wrapping the private key
   *   and OSKeyStore secret
   * @returns {Promise<Uint8Array>}
   *   The confirmation signature of the JSON block.
   */
  async confirm(meta, wrappedSecrets, salt, nonce) {
    let textEncoder = new TextEncoder();
    let metaBytes = textEncoder.encode(JSON.stringify(meta));
    let confirmation = new Uint8Array(
      await crypto.subtle.sign("HMAC", this.#authKey, metaBytes)
    );

    return {
      version: ArchiveUtils.SCHEMA_VERSION,
      encConfig: {
        wrappedSecrets: ArchiveUtils.arrayToBase64(wrappedSecrets),
        wrappedArchiveKeyMaterial: ArchiveUtils.arrayToBase64(
          this.#wrappedArchiveKeyMaterial
        ),
        salt: ArchiveUtils.arrayToBase64(salt),
        nonce: ArchiveUtils.arrayToBase64(nonce),
        confirmation: ArchiveUtils.arrayToBase64(confirmation),
      },
      meta,
    };
  }

  /**
   * Initializes an ArchiveEncryptor so that a caller can begin encrypting
   * chunks of a backup archive.
   *
   * @param {CryptoKey} publicKey
   *   The RSA-OAEP public key from an ArchiveEncryptionState.
   * @param {CryptoKey} backupAuthKey
   *   The AES-GCM BackupAuthKey from an ArchiveEncryptionState.
   * @returns {Promise<ArchiveEncryptor>}
   */
  static async initialize(publicKey, backupAuthKey) {
    ArchiveEncryptor.#isInternalConstructing = true;
    let instance = new ArchiveEncryptor();
    await instance.#initialize(publicKey, backupAuthKey);
    return instance;
  }
}

/**
 * A class that is used to decrypt one or more chunks of a backup archive.
 * Callers must use the async static initialize() method to create an
 * ArchiveDecryptor, and then can decrypt() individual chunks.
 */
export class ArchiveDecryptor {
  /**
   * A hack that lets us ensure that an ArchiveEncryptor cannot be
   * constructed except via the ArchiveEncryptor.initialize static
   * method.
   *
   * See https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Classes/Private_properties#simulating_private_constructors
   */
  static #isInternalConstructing = false;

  /**
   * The unwrapped RSA-OAEP private key extracted from the wrapped secrets of
   * a backup.
   *
   * @type {CryptoKey}
   */
  #privateKey = null;

  /**
   * The unique AES-GCM encryption key used to encrypt this particular backup,
   * derived from the wrappedArchiveKeyMaterial.
   *
   * @type {CryptoKey}
   */
  #archiveEncKey = null;

  /**
   * @see ArchiveDecryptor.OSKeyStoreSecret
   *
   * @type {string}
   */
  #OSKeyStoreSecret = null;

  /**
   * A big-endian counter nonce, incremented for each subsequent chunk of the
   * encrypted archive. The size of the nonce must be a multiple of 8 in order
   * to simplify the arithmetic via DataView / getBigUint64 / setBigUint64.
   *
   * @type {Uint8Array}
   */
  #nonce = new Uint8Array(16);

  /**
   * @see ArchiveDecryptor.#isInternalConstructing
   */
  constructor() {
    if (!ArchiveDecryptor.#isInternalConstructing) {
      throw new Error("ArchiveDecryptor is not constructable.");
    }
    ArchiveDecryptor.#isInternalConstructing = false;
  }

  /**
   * The unwrapped OSKeyStore secret that was stored within the JSON block.
   *
   * @type {string}
   */
  get OSKeyStoreSecret() {
    if (!this.isDone()) {
      throw new Error(
        "Cannot access OSKeyStoreSecret until all chunks are decrypted."
      );
    }
    return this.#OSKeyStoreSecret;
  }

  /**
   * Initializes an ArchiveDecryptor to decrypt a backup. This will throw if
   * the recovery code is not valid, or the meta property of the JSON block
   * appears to have been tampered with since signing. It is assumed that a
   * caller of this function has already validated that the JSON block has been
   * validated against the appropriate ArchiveJSONBlock JSON schema.
   *
   * @param {string} recoveryCode
   *   The recovery code originally used to encrypt the backup archive.
   * @param {object} jsonBlock
   *   The parsed JSON block that was stored with the backup archive. See the
   *   ArchiveJSONBlock JSON schema.
   */
  async #initialize(recoveryCode, jsonBlock) {
    if (jsonBlock.version > ArchiveUtils.SCHEMA_VERSION) {
      throw new Error(
        "JSON block version is greater than we can handle: ",
        jsonBlock.version
      );
    }

    let { encConfig, meta } = jsonBlock;
    let salt = ArchiveUtils.stringToArray(encConfig.salt);
    let nonce = ArchiveUtils.stringToArray(encConfig.nonce);
    let wrappedSecrets = ArchiveUtils.stringToArray(encConfig.wrappedSecrets);
    let wrappedArchiveKeyMaterial = ArchiveUtils.stringToArray(
      encConfig.wrappedArchiveKeyMaterial
    );
    let confirmation = ArchiveUtils.stringToArray(encConfig.confirmation);

    // First, recompute the BackupAuthKey and BackupEncKey from the recovery
    // code and salt
    let { backupAuthKey, backupEncKey } = await ArchiveUtils.computeBackupKeys(
      recoveryCode,
      salt
    );

    // Next, unwrap the secrets - the private RSA-OAEP key, and the
    // OSKeyStore secret.
    let unwrappedSecrets;
    try {
      unwrappedSecrets = new Uint8Array(
        await crypto.subtle.decrypt(
          {
            name: "AES-GCM",
            iv: nonce,
          },
          backupEncKey,
          wrappedSecrets
        )
      );
    } catch (e) {
      throw new Error("Unauthenticated");
    }

    let textDecoder = new TextDecoder();
    let secrets = JSON.parse(textDecoder.decode(unwrappedSecrets));

    this.#privateKey = await crypto.subtle.importKey(
      "jwk",
      secrets.privateKey,
      { name: "RSA-OAEP", hash: "SHA-256" },
      true /* extractable */,
      ["decrypt"]
    );

    this.#OSKeyStoreSecret = secrets.OSKeyStoreSecret;

    // Now use the private key to decrypt the wrappedArchiveKeyMaterial
    let archiveKeyMaterial = await crypto.subtle.decrypt(
      {
        name: "RSA-OAEP",
      },
      this.#privateKey,
      wrappedArchiveKeyMaterial
    );

    let { archiveEncKey, authKey } = await ArchiveUtils.computeEncryptionKeys(
      archiveKeyMaterial,
      backupAuthKey
    );

    this.#archiveEncKey = archiveEncKey;

    // Now ensure that the backup metadata has not been tampered with.
    let textEncoder = new TextEncoder();
    let jsonBlockBytes = textEncoder.encode(JSON.stringify(meta));
    let verified = await crypto.subtle.verify(
      "HMAC",
      authKey,
      confirmation,
      jsonBlockBytes
    );
    if (!verified) {
      this.#poisonSelf();
      throw new Error("Backup has been corrupted.");
    }
  }

  /**
   * Decrypts a chunk from a backup archive. This will throw if the cipherText
   * chunk appears to be too large (is greater than ARCHIVE_CHUNK_MAX)
   *
   * @param {Uint8Array} ciphertextChunk
   *   The ciphertext chunk of bytes to decrypt.
   * @param {boolean} [isLastChunk=false]
   *   Callers should set this to true if the chunk being decrypted is the
   *   last chunk. Once this is done, no additional chunks can be decrypted.
   * @returns {Promise<Uint8Array>}
   */
  async decrypt(ciphertextChunk, isLastChunk = false) {
    if (this.isDone()) {
      throw new Error(
        "Cannot decrypt any more chunks with this ArchiveDecryptor."
      );
    }

    if (
      ciphertextChunk.byteLength >
      ArchiveUtils.ARCHIVE_CHUNK_MAX_BYTES_SIZE + ArchiveUtils.TAG_LENGTH_BYTES
    ) {
      throw new Error(
        `Chunk is too large to decrypt: ${ciphertextChunk.byteLength} bytes`
      );
    }

    if (
      ciphertextChunk.byteLength !=
        ArchiveUtils.ARCHIVE_CHUNK_MAX_BYTES_SIZE +
          ArchiveUtils.TAG_LENGTH_BYTES &&
      !isLastChunk
    ) {
      throw new Error("Only last chunk can be smaller than the chunk max size");
    }

    if (isLastChunk) {
      NonceUtils.setLastChunkOnNonce(this.#nonce);
    }

    let plaintextChunk;

    try {
      plaintextChunk = await crypto.subtle.decrypt(
        {
          name: "AES-GCM",
          /**
           * Take only the last 12 bytes of the nonce, since the WebCrypto API
           * starts to behave differently when the IV is > 96 bits.
           */
          iv: this.#nonce.subarray(4),
          tagLength: ArchiveUtils.TAG_LENGTH,
        },
        this.#archiveEncKey,
        ciphertextChunk
      );
    } catch (e) {
      this.#poisonSelf();
      throw new Error("Failed to decrypt a chunk.");
    }

    NonceUtils.incrementNonce(this.#nonce);

    return new Uint8Array(plaintextChunk);
  }

  /**
   * Something has gone wrong during decryption. We want to make sure we cannot
   * possibly decrypt anything further, so we blow away our internal state,
   * effectively breaking this ArchiveDecryptor.
   */
  #poisonSelf() {
    this.#privateKey = null;
    this.#archiveEncKey = null;
    this.#OSKeyStoreSecret = null;
    this.#nonce = null;
  }

  /**
   * True if the last chunk flag has been set on the nonce already. Once this
   * returns true, no further chunks can be decrypted.
   *
   * @returns {boolean}
   */
  isDone() {
    return NonceUtils.lastChunkSetOnNonce(this.#nonce);
  }

  /**
   * Initializes an ArchiveDecryptor using the recovery code and the JSON
   * block that was extracted from the archive. The caller is expected to have
   * already checked that the JSON block adheres to the ArchiveJSONBlock
   * schema. The initialization may fail, and the Promise rejected, if the
   * recovery code is not correct, or the meta data of the JSON block has
   * changed since it was signed.
   *
   * @param {string} recoveryCode
   *   The recovery code to attempt to begin decryption with.
   * @param {object} jsonBlock
   *   See the ArchiveJSONBlock schema for details.
   * @returns {Promise<ArchiveDecryptor>}
   */
  static async initialize(recoveryCode, jsonBlock) {
    ArchiveDecryptor.#isInternalConstructing = true;
    let instance = new ArchiveDecryptor();
    await instance.#initialize(recoveryCode, jsonBlock);
    return instance;
  }
}
