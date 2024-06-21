/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ArchiveEncryptionState } = ChromeUtils.importESModule(
  "resource:///modules/backup/ArchiveEncryptionState.sys.mjs"
);
const { OSKeyStoreTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/OSKeyStoreTestUtils.sys.mjs"
);
const { ArchiveUtils } = ChromeUtils.importESModule(
  "resource:///modules/backup/ArchiveUtils.sys.mjs"
);
const { ArchiveDecryptor } = ChromeUtils.importESModule(
  "resource:///modules/backup/ArchiveEncryption.sys.mjs"
);
const { DecoderDecryptorTransformer, FileWriterStream } =
  ChromeUtils.importESModule(
    "resource:///modules/backup/BackupService.sys.mjs"
  );

let testProfilePath;
let fakeCompressedStagingPath;
let archiveTemplateFile = do_get_file("data/test_archive.template.html");
let archiveTemplateURI = Services.io.newFileURI(archiveTemplateFile).spec;

const SIZE_IN_BYTES = 125123;
let fakeBytes;

async function assertExtractionsMatch(extractionPath) {
  let writtenBytes = await IOUtils.read(extractionPath);
  assertUint8ArraysSimilarity(
    writtenBytes,
    fakeBytes,
    true /* expectSimilar */
  );
}

add_setup(async () => {
  testProfilePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "testCreateArchive"
  );

  fakeCompressedStagingPath = PathUtils.join(
    testProfilePath,
    "fake-compressed-staging.zip"
  );

  // Let's create a large chunk of nonsense data that we can pretend is the
  // compressed archive just to make sure that we can get it back out again.
  // Instead of putting a large file inside of version control, we
  // deterministically generate some nonsense data inside of a Uint8Array to
  // encode. Generating the odd positive integer sequence seems like a decent
  // enough mechanism for deterministically generating nonsense data. We ensure
  // that the number of bytes written is not a multiple of 6 so that we can
  // ensure that base64 padding is working.
  fakeBytes = new Uint8Array(SIZE_IN_BYTES);

  // seededRandomNumberGenerator is defined in head.js, but eslint doesn't seem
  // happy about it. Maybe that's because it's a generator function.
  // eslint-disable-next-line no-undef
  let gen = seededRandomNumberGenerator();
  for (let i = 0; i < SIZE_IN_BYTES; ++i) {
    fakeBytes.set(gen.next().value, i);
  }

  await IOUtils.write(fakeCompressedStagingPath, fakeBytes);

  OSKeyStoreTestUtils.setup();

  registerCleanupFunction(async () => {
    await OSKeyStoreTestUtils.cleanup();
    await IOUtils.remove(testProfilePath, { recursive: true });
  });
});

/**
 * Tests that a single-file archive can be created from some file on the
 * file system and not be encrypted. This is a bit more integration-y, since
 * it's also testing the Archive.worker.mjs script - but that script is
 * basically an extension of createArchive that lets it operate off of the
 * main thread.
 */
add_task(async function test_createArchive_unencrypted() {
  let bs = new BackupService();

  const FAKE_ARCHIVE_PATH = PathUtils.join(
    testProfilePath,
    "fake-unencrypted-archive.html"
  );

  await bs.createArchive(
    FAKE_ARCHIVE_PATH,
    archiveTemplateURI,
    fakeCompressedStagingPath,
    null /* no ArchiveEncryptionState */,
    FAKE_METADATA
  );

  let { isEncrypted, archiveJSON } = await bs.sampleArchive(FAKE_ARCHIVE_PATH);
  Assert.ok(!isEncrypted, "Should not be considered encrypted.");
  Assert.deepEqual(
    archiveJSON.meta,
    FAKE_METADATA,
    "Metadata was encoded in the archive JSON block."
  );

  const EXTRACTION_PATH = PathUtils.join(testProfilePath, "extraction.bin");
  await bs.extractCompressedSnapshotFromArchive(
    FAKE_ARCHIVE_PATH,
    EXTRACTION_PATH
  );

  assertExtractionsMatch(EXTRACTION_PATH);

  await IOUtils.remove(FAKE_ARCHIVE_PATH);
  await IOUtils.remove(EXTRACTION_PATH);
});

/**
 * Tests that a single-file archive can be created from some file on the
 * file system and be encrypted and decrypted. This is a bit more integration-y,
 * since it's also testing the Archive.worker.mjs script - but that script is
 * basically an extension of createArchive that lets it operate off of the
 * main thread.
 */
add_task(async function test_createArchive_encrypted() {
  const TEST_RECOVERY_CODE = "This is some recovery code.";

  let bs = new BackupService();
  let { instance: encState } = await ArchiveEncryptionState.initialize(
    TEST_RECOVERY_CODE
  );

  const FAKE_ARCHIVE_PATH = PathUtils.join(
    testProfilePath,
    "fake-encrypted-archive.html"
  );

  await bs.createArchive(
    FAKE_ARCHIVE_PATH,
    archiveTemplateURI,
    fakeCompressedStagingPath,
    encState,
    FAKE_METADATA
  );

  let { isEncrypted, archiveJSON } = await bs.sampleArchive(FAKE_ARCHIVE_PATH);
  Assert.ok(isEncrypted, "Should be considered encrypted.");
  Assert.deepEqual(
    archiveJSON.meta,
    FAKE_METADATA,
    "Metadata was encoded in the archive JSON block."
  );

  const EXTRACTION_PATH = PathUtils.join(testProfilePath, "extraction.bin");

  // This should fail, since the archive is encrypted.
  await Assert.rejects(
    bs.extractCompressedSnapshotFromArchive(FAKE_ARCHIVE_PATH, EXTRACTION_PATH),
    /recovery code is required/
  );

  await bs.extractCompressedSnapshotFromArchive(
    FAKE_ARCHIVE_PATH,
    EXTRACTION_PATH,
    TEST_RECOVERY_CODE
  );

  assertExtractionsMatch(EXTRACTION_PATH);

  await IOUtils.remove(FAKE_ARCHIVE_PATH);
  await IOUtils.remove(EXTRACTION_PATH);
});

/**
 * Tests that an archive can be created where the bytes of the archive are
 * a multiple of 6, but the individual chunks of those bytes are not a multiple
 * of 6 (which will necessitate base64 padding).
 */
add_task(async function test_createArchive_multiple_of_six_test() {
  let bs = new BackupService();

  const FAKE_ARCHIVE_PATH = PathUtils.join(
    testProfilePath,
    "fake-unencrypted-archive.html"
  );
  const FAKE_COMPRESSED_FILE = PathUtils.join(
    testProfilePath,
    "fake-compressed-staging-mul6.zip"
  );

  // Instead of generating a gigantic chunk of data to test this particular
  // case, we'll override the default chunk size. We'll choose a chunk size of
  // 500 bytes, which doesn't divide evenly by 6 - but we'll encode a set of
  // 6 * 500 bytes, which will naturally divide evenly by 6.
  const NOT_MULTIPLE_OF_SIX_OVERRIDE_CHUNK_SIZE = 500;
  const MULTIPLE_OF_SIX_SIZE_IN_BYTES = 6 * 500;
  let multipleOfSixBytes = new Uint8Array(MULTIPLE_OF_SIX_SIZE_IN_BYTES);

  // seededRandomNumberGenerator is defined in head.js, but eslint doesn't seem
  // happy about it. Maybe that's because it's a generator function.
  // eslint-disable-next-line no-undef
  let gen = seededRandomNumberGenerator();
  for (let i = 0; i < MULTIPLE_OF_SIX_SIZE_IN_BYTES; ++i) {
    multipleOfSixBytes.set(gen.next().value, i);
  }

  await IOUtils.write(FAKE_COMPRESSED_FILE, multipleOfSixBytes);

  await bs.createArchive(
    FAKE_ARCHIVE_PATH,
    archiveTemplateURI,
    FAKE_COMPRESSED_FILE,
    null /* no ArchiveEncryptionState */,
    FAKE_METADATA,
    {
      chunkSize: NOT_MULTIPLE_OF_SIX_OVERRIDE_CHUNK_SIZE,
    }
  );

  const EXTRACTION_PATH = PathUtils.join(testProfilePath, "extraction.bin");
  await bs.extractCompressedSnapshotFromArchive(
    FAKE_ARCHIVE_PATH,
    EXTRACTION_PATH
  );

  let writtenBytes = await IOUtils.read(EXTRACTION_PATH);
  assertUint8ArraysSimilarity(
    writtenBytes,
    multipleOfSixBytes,
    true /* expectSimilar */
  );

  await IOUtils.remove(FAKE_COMPRESSED_FILE);
  await IOUtils.remove(FAKE_ARCHIVE_PATH);
  await IOUtils.remove(EXTRACTION_PATH);
});

/**
 * Tests that if an encrypted single-file archive has had its binary blob
 * truncated that the decryption fails and the recovery.zip file is
 * automatically destroyed.
 */
add_task(async function test_createArchive_encrypted_truncated() {
  const TEST_RECOVERY_CODE = "This is some recovery code.";

  let bs = new BackupService();
  let { instance: encState } = await ArchiveEncryptionState.initialize(
    TEST_RECOVERY_CODE
  );

  const FAKE_ARCHIVE_PATH = PathUtils.join(
    testProfilePath,
    "fake-encrypted-archive.html"
  );
  const FAKE_COMPRESSED_FILE = PathUtils.join(
    testProfilePath,
    "fake-compressed-staging-large.zip"
  );

  const MULTIPLE_OF_MAX_CHUNK_SIZE =
    2 * ArchiveUtils.ARCHIVE_CHUNK_MAX_BYTES_SIZE;
  let multipleOfMaxChunkSizeBytes = new Uint8Array(MULTIPLE_OF_MAX_CHUNK_SIZE);
  // seededRandomNumberGenerator is defined in head.js, but eslint doesn't seem
  // happy about it. Maybe that's because it's a generator function.
  // eslint-disable-next-line no-undef
  let gen = seededRandomNumberGenerator();
  for (let i = 0; i < MULTIPLE_OF_MAX_CHUNK_SIZE; ++i) {
    multipleOfMaxChunkSizeBytes.set(gen.next().value, i);
  }

  await IOUtils.write(FAKE_COMPRESSED_FILE, multipleOfMaxChunkSizeBytes);

  await bs.createArchive(
    FAKE_ARCHIVE_PATH,
    archiveTemplateURI,
    FAKE_COMPRESSED_FILE,
    encState,
    FAKE_METADATA
  );

  // This is a little bit gross - we're going to read out the data from the
  // generated file, find the last line longer than ARCHIVE_CHUNK_MAX_BYTES_SIZE
  // (which should be the last base64 encoded value), and then splice it out,
  // before flushing that change back to disk.
  let lines = (await IOUtils.readUTF8(FAKE_ARCHIVE_PATH)).split("\n");
  let foundIndex = -1;
  // The longest lines will be the base64 encoded chunks. Remove the last one.
  for (let i = lines.length - 1; i >= 0; i--) {
    if (lines[i].length > ArchiveUtils.ARCHIVE_CHUNK_MAX_BYTES_SIZE) {
      foundIndex = i;
      break;
    }
  }
  Assert.notEqual(foundIndex, -1, "Should have found a long line");
  lines.splice(foundIndex, 1);
  await IOUtils.writeUTF8(FAKE_ARCHIVE_PATH, lines.join("\n"));

  let { isEncrypted } = await bs.sampleArchive(FAKE_ARCHIVE_PATH);
  Assert.ok(isEncrypted, "Should be considered encrypted.");

  const EXTRACTION_PATH = PathUtils.join(testProfilePath, "extraction.bin");

  await Assert.rejects(
    bs.extractCompressedSnapshotFromArchive(
      FAKE_ARCHIVE_PATH,
      EXTRACTION_PATH,
      TEST_RECOVERY_CODE
    ),
    /Corrupted archive/
  );

  Assert.ok(
    !(await IOUtils.exists(EXTRACTION_PATH)),
    "Extraction should have been automatically destroyed."
  );

  await IOUtils.remove(FAKE_ARCHIVE_PATH);
  await IOUtils.remove(FAKE_COMPRESSED_FILE);
});

/**
 * Tests that if the BinaryReadableStream closes early before the last chunk
 * is decrypted, that the recovery file is destroyed.
 */
add_task(async function test_createArchive_early_binary_stream_close() {
  const TEST_RECOVERY_CODE = "This is some recovery code.";

  let bs = new BackupService();
  let { instance: encState } = await ArchiveEncryptionState.initialize(
    TEST_RECOVERY_CODE
  );

  const FAKE_ARCHIVE_PATH = PathUtils.join(
    testProfilePath,
    "fake-encrypted-archive.html"
  );

  await bs.createArchive(
    FAKE_ARCHIVE_PATH,
    archiveTemplateURI,
    fakeCompressedStagingPath,
    encState,
    FAKE_METADATA
  );

  let { isEncrypted, startByteOffset, contentType, archiveJSON } =
    await bs.sampleArchive(FAKE_ARCHIVE_PATH);
  Assert.ok(isEncrypted, "Should be considered encrypted.");

  let archiveFile = await IOUtils.getFile(FAKE_ARCHIVE_PATH);
  let archiveStream = await bs.createBinaryReadableStream(
    archiveFile,
    startByteOffset,
    contentType
  );
  let decryptor = await ArchiveDecryptor.initialize(
    TEST_RECOVERY_CODE,
    archiveJSON
  );
  const EXTRACTION_PATH = PathUtils.join(testProfilePath, "extraction.bin");

  let binaryDecoder = new TransformStream(
    new DecoderDecryptorTransformer(decryptor)
  );
  let fileWriter = new WritableStream(
    new FileWriterStream(EXTRACTION_PATH, decryptor)
  );

  // We're going to run the characters from the archiveStream through an
  // intermediary TransformStream that is going to cause an abort before the
  // the stream can complete. We'll do that by only passing part of the first
  // chunk through, and then aborting.
  let earlyAborter = new TransformStream({
    async transform(chunkPart, controller) {
      controller.enqueue(
        chunkPart.substring(0, Math.floor(chunkPart.length / 2))
      );
      controller.error("We're done. Aborting early.");
    },
  });

  let pipePromise = archiveStream
    .pipeThrough(earlyAborter)
    .pipeThrough(binaryDecoder)
    .pipeTo(fileWriter);

  await Assert.rejects(pipePromise, /Aborting early/);
  Assert.ok(
    !(await IOUtils.exists(EXTRACTION_PATH)),
    "Extraction should have been automatically destroyed."
  );

  await IOUtils.remove(FAKE_ARCHIVE_PATH);
});
