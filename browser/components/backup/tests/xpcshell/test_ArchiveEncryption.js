/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ArchiveEncryptionState } = ChromeUtils.importESModule(
  "resource:///modules/backup/ArchiveEncryptionState.sys.mjs"
);
const { ArchiveEncryptor, ArchiveDecryptor } = ChromeUtils.importESModule(
  "resource:///modules/backup/ArchiveEncryption.sys.mjs"
);
const { ArchiveUtils } = ChromeUtils.importESModule(
  "resource:///modules/backup/ArchiveUtils.sys.mjs"
);

const TEST_RECOVERY_CODE = "This is my recovery code.";

const FAKE_BYTES_AMOUNT = 1000;

let fakeBytes = null;

add_setup(async () => {
  fakeBytes = new Uint8Array(FAKE_BYTES_AMOUNT);
  // seededRandomNumberGenerator is defined in head.js, but eslint doesn't seem
  // happy about it. Maybe that's because it's a generator function.
  // eslint-disable-next-line no-undef
  let gen = seededRandomNumberGenerator();
  for (let i = 0; i < FAKE_BYTES_AMOUNT; ++i) {
    fakeBytes.set(gen.next().value, i);
  }
});

/**
 * Tests that we can construct an ArchiveEncryptor by way of the properties
 * of an ArchiveEncryptionState.
 */
add_task(async function test_ArchiveEncryptor_initializer() {
  let { instance: encState } =
    await ArchiveEncryptionState.initialize(TEST_RECOVERY_CODE);
  let encryptor = await ArchiveEncryptor.initialize(
    encState.publicKey,
    encState.backupAuthKey
  );
  Assert.ok(encryptor, "An ArchiveEncryptor was successfully constructed");
});

/**
 * Tests that we can encrypt a single chunk of bytes.
 */
add_task(async function test_ArchiveEncryption_single_chunk() {
  let { instance: encState } =
    await ArchiveEncryptionState.initialize(TEST_RECOVERY_CODE);
  let encryptor = await ArchiveEncryptor.initialize(
    encState.publicKey,
    encState.backupAuthKey
  );

  const TEST_METADATA = { test: "hello!" };
  let jsonBlock = await encryptor.confirm(
    TEST_METADATA,
    encState.wrappedSecrets,
    encState.salt,
    encState.nonce
  );
  // Ensure that the JSON block can be serialized to string, and deserialized
  // again.
  jsonBlock = JSON.parse(JSON.stringify(jsonBlock));

  let encryptedBytes = await encryptor.encrypt(
    fakeBytes,
    true /* isLastChunk */
  );

  // Ensure the the encrypted bytes do not match the plaintext bytes.
  Assert.greater(
    encryptedBytes.byteLength,
    fakeBytes.byteLength,
    "Encrypted bytes should be larger"
  );

  assertUint8ArraysSimilarity(
    encryptedBytes,
    fakeBytes,
    false /* expectSimilar */
  );

  let decryptor = await ArchiveDecryptor.initialize(
    TEST_RECOVERY_CODE,
    jsonBlock
  );
  Assert.ok(decryptor, "Got back an initialized ArchiveDecryptor");

  let decryptedBytes = await decryptor.decrypt(encryptedBytes, true);

  Assert.equal(
    decryptedBytes.byteLength,
    fakeBytes.byteLength,
    "Decrypted bytes should have original length"
  );

  assertUint8ArraysSimilarity(
    decryptedBytes,
    fakeBytes,
    true /* expectSimilar */
  );
});

/**
 * Tests that we can encrypt an unevenly sized set of chunks.
 */
add_task(async function test_ArchiveEncryption_uneven_chunks() {
  let { instance: encState } =
    await ArchiveEncryptionState.initialize(TEST_RECOVERY_CODE);
  let encryptor = await ArchiveEncryptor.initialize(
    encState.publicKey,
    encState.backupAuthKey
  );

  const TEST_METADATA = { test: "hello!" };
  let jsonBlock = await encryptor.confirm(
    TEST_METADATA,
    encState.wrappedSecrets,
    encState.salt,
    encState.nonce
  );
  // Ensure that the JSON block can be serialized to string, and deserialized
  // again.
  jsonBlock = JSON.parse(JSON.stringify(jsonBlock));

  // FAKE_BYTES_AMOUNT / 3 shouldn't divide cleanly. So our chunks will have the
  // following byte indices:
  //
  // - 0, 332 (333 bytes)
  // - 333, 666 (333 bytes)
  // - 667, 999 (332 bytes)
  //
  // Note that subarray's "end" argument is _exclusive_.
  let sandbox = sinon.createSandbox();
  sandbox.stub(ArchiveUtils, "ARCHIVE_CHUNK_MAX_BYTES_SIZE").get(() => {
    return 333;
  });

  let firstChunk = fakeBytes.subarray(0, 333);
  Assert.equal(firstChunk.byteLength, 333);
  let secondChunk = fakeBytes.subarray(333, 666);
  Assert.equal(secondChunk.byteLength, 333);
  let thirdChunk = fakeBytes.subarray(667, 999);
  Assert.equal(thirdChunk.byteLength, 332);

  let encryptedFirstChunk = await encryptor.encrypt(firstChunk);
  let encryptedSecondChunk = await encryptor.encrypt(secondChunk);
  let encryptedThirdChunk = await encryptor.encrypt(
    thirdChunk,
    true /*isLastChunk */
  );

  let encryptedPairsToCompare = [
    [firstChunk, encryptedFirstChunk],
    [secondChunk, encryptedSecondChunk],
    [thirdChunk, encryptedThirdChunk],
  ];

  for (let [chunk, encryptedChunk] of encryptedPairsToCompare) {
    assertUint8ArraysSimilarity(
      chunk,
      encryptedChunk,
      false /* expectSimilar */
    );
  }

  let decryptor = await ArchiveDecryptor.initialize(
    TEST_RECOVERY_CODE,
    jsonBlock
  );
  Assert.ok(decryptor, "Got back an initialized ArchiveDecryptor");

  let decryptedFirstChunk = await decryptor.decrypt(encryptedFirstChunk);
  let decryptedSecondChunk = await decryptor.decrypt(encryptedSecondChunk);
  let decryptedThirdChunk = await decryptor.decrypt(
    encryptedThirdChunk,
    true /* isLastChunk */
  );

  let decryptedPairsToCompare = [
    [firstChunk, decryptedFirstChunk],
    [secondChunk, decryptedSecondChunk],
    [thirdChunk, decryptedThirdChunk],
  ];

  for (let [chunk, decryptedChunk] of decryptedPairsToCompare) {
    Assert.equal(
      chunk.byteLength,
      decryptedChunk.byteLength,
      "Decrypted bytes should have original length"
    );
    assertUint8ArraysSimilarity(
      chunk,
      decryptedChunk,
      true /* expectSimilar */
    );
  }
  sandbox.restore();
});

/**
 * Tests that we can encrypt an even sized set of chunks.
 */
add_task(async function test_ArchiveEncryption_even_chunks() {
  let { instance: encState } =
    await ArchiveEncryptionState.initialize(TEST_RECOVERY_CODE);
  let encryptor = await ArchiveEncryptor.initialize(
    encState.publicKey,
    encState.backupAuthKey
  );

  const TEST_METADATA = { test: "hello!" };
  let jsonBlock = await encryptor.confirm(
    TEST_METADATA,
    encState.wrappedSecrets,
    encState.salt,
    encState.nonce
  );
  // Ensure that the JSON block can be serialized to string, and deserialized
  // again.
  jsonBlock = JSON.parse(JSON.stringify(jsonBlock));

  // FAKE_BYTES_AMOUNT / 2 should divide evenly. So our chunks will have the
  // following byte indices:
  //
  // - 0, 499 (500 bytes)
  // - 500, 999 (500 bytes)
  //
  // Note that subarray's "end" argument is _exclusive_.
  let sandbox = sinon.createSandbox();
  sandbox.stub(ArchiveUtils, "ARCHIVE_CHUNK_MAX_BYTES_SIZE").get(() => {
    return 500;
  });

  let firstChunk = fakeBytes.subarray(0, 500);
  Assert.equal(firstChunk.byteLength, 500);
  let secondChunk = fakeBytes.subarray(500);
  Assert.equal(secondChunk.byteLength, 500);

  let encryptedFirstChunk = await encryptor.encrypt(firstChunk);
  let encryptedSecondChunk = await encryptor.encrypt(
    secondChunk,
    true /*isLastChunk */
  );

  let encryptedPairsToCompare = [
    [firstChunk, encryptedFirstChunk],
    [secondChunk, encryptedSecondChunk],
  ];

  for (let [chunk, encryptedChunk] of encryptedPairsToCompare) {
    assertUint8ArraysSimilarity(
      chunk,
      encryptedChunk,
      false /* expectSimilar */
    );
  }

  let decryptor = await ArchiveDecryptor.initialize(
    TEST_RECOVERY_CODE,
    jsonBlock
  );
  Assert.ok(decryptor, "Got back an initialized ArchiveDecryptor");

  let decryptedFirstChunk = await decryptor.decrypt(encryptedFirstChunk);
  let decryptedSecondChunk = await decryptor.decrypt(
    encryptedSecondChunk,
    true /* isLastChunk */
  );

  let decryptedPairsToCompare = [
    [firstChunk, decryptedFirstChunk],
    [secondChunk, decryptedSecondChunk],
  ];

  for (let [chunk, decryptedChunk] of decryptedPairsToCompare) {
    Assert.equal(
      chunk.byteLength,
      decryptedChunk.byteLength,
      "Decrypted bytes should have original length"
    );
    assertUint8ArraysSimilarity(
      chunk,
      decryptedChunk,
      true /* expectSimilar */
    );
  }
  sandbox.restore();
});

/**
 * Tests that we cannot decrypt with the wrong recovery code.
 */
add_task(async function test_ArchiveEncryption_wrong_recoveryCode() {
  let { instance: encState } =
    await ArchiveEncryptionState.initialize(TEST_RECOVERY_CODE);
  let encryptor = await ArchiveEncryptor.initialize(
    encState.publicKey,
    encState.backupAuthKey
  );

  const TEST_METADATA = { test: "hello!" };
  let jsonBlock = await encryptor.confirm(
    TEST_METADATA,
    encState.wrappedSecrets,
    encState.salt,
    encState.nonce
  );

  // We don't actually care about the encrypted bytes, since we're just
  // testing that ArchiveDecryptor won't accept an incorrect recovery code.
  await encryptor.encrypt(fakeBytes, true /* isLastChunk */);

  await Assert.rejects(
    ArchiveDecryptor.initialize("Wrong recovery code", jsonBlock),
    /Unauthenticated/
  );
});
