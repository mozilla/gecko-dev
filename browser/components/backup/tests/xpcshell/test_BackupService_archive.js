/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that a single-file archive can be created from some file on the
 * file system and not be encrypted. This is a bit more integration-y, since
 * it's also testing the Archive.worker.mjs script - but that script is
 * basically an extension of createArchive that lets it operate off of the
 * main thread.
 */
add_task(async function test_createArchive_unencrypted() {
  let bs = new BackupService();
  let testProfilePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "testCreateArchive"
  );

  let fakeMetadata = {
    testKey: "test value",
  };

  let archiveTemplateFile = do_get_file("data/test_archive.template.html");
  let archiveTemplateURI = Services.io.newFileURI(archiveTemplateFile).spec;

  const FAKE_COMPRESSED_STAGING_PATH = PathUtils.join(
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
  const SIZE_IN_BYTES = 1525231;
  let fakeBytes = new Uint8Array(SIZE_IN_BYTES);

  // seededRandomNumberGenerator is defined in head.js, but eslint doesn't seem
  // happy about it. Maybe that's because it's a generator function.
  // eslint-disable-next-line no-undef
  let gen = seededRandomNumberGenerator();
  for (let i = 0; i < SIZE_IN_BYTES; ++i) {
    fakeBytes.set([gen.next().value % 255], i);
  }

  await IOUtils.write(FAKE_COMPRESSED_STAGING_PATH, fakeBytes);

  const FAKE_ARCHIVE_PATH = PathUtils.join(
    testProfilePath,
    "fake-archive.html"
  );

  await bs.createArchive(
    FAKE_ARCHIVE_PATH,
    archiveTemplateURI,
    FAKE_COMPRESSED_STAGING_PATH,
    null /* no ArchiveEncryptionState */,
    fakeMetadata
  );

  const EXTRACTION_PATH = PathUtils.join(testProfilePath, "extraction.bin");
  await bs.extractCompressedSnapshotFromArchive(
    FAKE_ARCHIVE_PATH,
    EXTRACTION_PATH
  );

  // Now open the extraction and compare the results.
  Assert.equal((await IOUtils.stat(EXTRACTION_PATH)).size, SIZE_IN_BYTES);
  let writtenBytes = await IOUtils.read(EXTRACTION_PATH);
  let matches = true;
  for (let i = 0; i < writtenBytes.byteLength; ++i) {
    if (writtenBytes[i] !== fakeBytes[i]) {
      Assert.ok(false, "Byte at index " + i + " did not match.");
      matches = false;
    }
  }
  Assert.ok(matches, "All bytes matched after extraction.");

  await IOUtils.remove(testProfilePath, { recursive: true });
});
