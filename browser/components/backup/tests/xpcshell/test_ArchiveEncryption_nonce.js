/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ArchiveUtils } = ChromeUtils.importESModule(
  "resource:///modules/backup/ArchiveUtils.sys.mjs"
);
const { NonceUtils } = ChromeUtils.importESModule(
  "resource:///modules/backup/ArchiveEncryption.sys.mjs"
);

/**
 * Tests that we can increment a nonce and set the last chunk byte.
 */
add_task(async function test_nonce_arithmetic() {
  let nonce = new Uint8Array(16);

  NonceUtils.incrementNonce(nonce);
  Assert.deepEqual(
    nonce,
    new Uint8Array([0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1])
  );

  NonceUtils.incrementNonce(nonce, 255);
  Assert.deepEqual(
    nonce,
    new Uint8Array([0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0])
  );

  NonceUtils.incrementNonce(nonce);
  Assert.deepEqual(
    nonce,
    new Uint8Array([0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1])
  );

  NonceUtils.incrementNonce(nonce, 255);
  Assert.deepEqual(
    nonce,
    new Uint8Array([0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0])
  );

  NonceUtils.incrementNonce(nonce, 257);
  Assert.deepEqual(
    nonce,
    new Uint8Array([0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1])
  );

  Assert.ok(
    !NonceUtils.lastChunkSetOnNonce(nonce),
    "Last chunk bit hasn't been flipped yet."
  );

  // When marking the last chunk on the nonce, we set the 12th byte of the
  // counter to 0x01.
  NonceUtils.setLastChunkOnNonce(nonce);
  Assert.deepEqual(
    nonce,
    new Uint8Array([0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1])
  );
  Assert.ok(
    NonceUtils.lastChunkSetOnNonce(nonce),
    "Last chunk bit was flipped."
  );
});

/**
 * Tests that the nonce counter will throw if it is incremented past a value
 * indicating a greater number of bytes in the backup than
 * ArchiveUtils.ARCHIVE_MAX_BYTES_SIZE.
 */
add_task(async function test_exceed_size() {
  let nonce = new Uint8Array(16);

  // Get us right up to the limit. Increasing by one past this value should
  // cause us to throw.
  NonceUtils.incrementNonce(
    nonce,
    ArchiveUtils.ARCHIVE_MAX_BYTES_SIZE /
      ArchiveUtils.ARCHIVE_CHUNK_MAX_BYTES_SIZE
  );

  Assert.throws(() => {
    NonceUtils.incrementNonce(nonce);
  }, /Exceeded/);
});
