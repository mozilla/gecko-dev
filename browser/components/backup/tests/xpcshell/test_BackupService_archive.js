/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ArchiveEncryptionState } = ChromeUtils.importESModule(
  "resource:///modules/backup/ArchiveEncryptionState.sys.mjs"
);
const { OSKeyStoreTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/OSKeyStoreTestUtils.sys.mjs"
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
