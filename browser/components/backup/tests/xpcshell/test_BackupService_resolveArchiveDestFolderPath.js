/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

const HOME_KEY = "Home";
let gTestRoot;
let gFakeHomePath;
let gFakeHomeFile;

add_setup(async () => {
  gTestRoot = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "testResolveArchiveDestFolderPath"
  );
  gFakeHomePath = PathUtils.join(gTestRoot, "FakeHome");
  await IOUtils.makeDirectory(gFakeHomePath);

  gFakeHomeFile = await IOUtils.getFile(gFakeHomePath);

  let dirsvc = Services.dirsvc.QueryInterface(Ci.nsIProperties);
  let originalFile;
  try {
    originalFile = dirsvc.get(HOME_KEY, Ci.nsIFile);
    dirsvc.undefine(HOME_KEY);
  } catch (e) {
    // dirsvc.get will throw if nothing provides for the key and dirsvc.undefine
    // will throw if it's not a persistent entry, in either case we don't want
    // to set the original file in cleanup.
    originalFile = undefined;
  }

  dirsvc.set(HOME_KEY, gFakeHomeFile);
  registerCleanupFunction(() => {
    dirsvc.undefine(HOME_KEY);
    if (originalFile) {
      dirsvc.set(HOME_KEY, originalFile);
    }
  });
});

/**
 * Tests that we create the destination folder if the parent folder exists
 * and the destination folder does not.
 */
add_task(async function test_create_folder() {
  const PARENT_FOLDER = PathUtils.join(gTestRoot, "TestFolder");
  await IOUtils.makeDirectory(PARENT_FOLDER);
  let bs = new BackupService();

  const DESTINATION_PATH = PathUtils.join(
    PARENT_FOLDER,
    BackupService.BACKUP_DIR_NAME
  );
  let path = await bs.resolveArchiveDestFolderPath(DESTINATION_PATH);

  Assert.equal(path, DESTINATION_PATH, "Got back the expected folder path.");
  Assert.ok(await IOUtils.exists(path), "The destination folder was created.");
  Assert.equal(
    (await IOUtils.getChildren(path)).length,
    0,
    "Destination folder should be empty."
  );
  await IOUtils.remove(PARENT_FOLDER, { recursive: true });
});

/**
 * Tests that we will recreate the configured destination folder if the parent
 * folder does not exist. This recreates the entire configured folder
 * hierarchy.
 */
add_task(async function test_create_parent_folder_hierarchy() {
  const MISSING_PARENT_FOLDER = PathUtils.join(gTestRoot, "DoesNotExistYet");
  Assert.ok(
    !(await IOUtils.exists(MISSING_PARENT_FOLDER)),
    "Folder should not exist yet."
  );
  let bs = new BackupService();

  const CONFIGURED_DESTINATION_PATH = PathUtils.join(
    MISSING_PARENT_FOLDER,
    BackupService.BACKUP_DIR_NAME
  );
  let path = await bs.resolveArchiveDestFolderPath(CONFIGURED_DESTINATION_PATH);
  Assert.equal(
    path,
    CONFIGURED_DESTINATION_PATH,
    "Got back the expected folder path."
  );
  Assert.ok(await IOUtils.exists(path), "The destination folder was created.");

  await IOUtils.remove(MISSING_PARENT_FOLDER, { recursive: true });
});

/**
 * Tests that we return the destination folder if the parent folder exists
 * along with the destination folder.
 */
add_task(async function test_find_folder() {
  const PARENT_FOLDER = PathUtils.join(gTestRoot, "TestFolder");
  const DESTINATION_PATH = PathUtils.join(
    PARENT_FOLDER,
    BackupService.BACKUP_DIR_NAME
  );
  await IOUtils.makeDirectory(DESTINATION_PATH, { createAncestors: true });

  let bs = new BackupService();
  let path = await bs.resolveArchiveDestFolderPath(DESTINATION_PATH);

  Assert.equal(path, DESTINATION_PATH, "Got back the expected folder path.");
  Assert.ok(await IOUtils.exists(path), "The destination folder exists.");
  Assert.equal(
    (await IOUtils.getChildren(path)).length,
    0,
    "Destination folder should be empty."
  );
  await IOUtils.remove(PARENT_FOLDER, { recursive: true });
});

/**
 * Tests that we fall back to the DEFAULT_PARENT_DIR_PATH folder if the
 * configured path cannot be written to. This might happen if, for example, the
 * configured destination is a removable drive that has been removed.
 */
add_task(async function test_fallback_to_default() {
  if (AppConstants.platform == "win") {
    todo_check_true(
      false,
      "Programmatically setting folder permissions does not work on " +
        "Windows, so this test is skipped."
    );
    return;
  }

  const UNWRITABLE_PARENT = PathUtils.join(gTestRoot, "UnwritableParent");
  await IOUtils.makeDirectory(UNWRITABLE_PARENT);
  // Make the folder read-only across the board. 0o444 is the chmod numeric code
  // for that.
  await IOUtils.setPermissions(UNWRITABLE_PARENT, 0o444);

  const CONFIGURED_FOLDER = PathUtils.join(
    UNWRITABLE_PARENT,
    "ImpossibleChild"
  );
  Assert.ok(
    !(await IOUtils.exists(CONFIGURED_FOLDER)),
    "Configured folder should not exist."
  );

  const DEFAULT_FOLDER = PathUtils.join(gTestRoot, "FakeDocuments");
  await IOUtils.makeDirectory(DEFAULT_FOLDER);

  let bs = new BackupService();
  // Stub out the DEFAULT_PARENT_DIR_PATH into a folder path we control in this
  // test, so that we don't pollute this machine's actual Documents folder.
  let sandbox = sinon.createSandbox();
  sandbox
    .stub(BackupService, "DEFAULT_PARENT_DIR_PATH")
    .get(() => DEFAULT_FOLDER);

  const CONFIGURED_DESTINATION_PATH = PathUtils.join(
    CONFIGURED_FOLDER,
    BackupService.BACKUP_DIR_NAME
  );
  const EXPECTED_DESTINATION_PATH = PathUtils.join(
    DEFAULT_FOLDER,
    BackupService.BACKUP_DIR_NAME
  );
  let path = await bs.resolveArchiveDestFolderPath(CONFIGURED_DESTINATION_PATH);
  Assert.equal(
    path,
    EXPECTED_DESTINATION_PATH,
    "Got back the expected folder path."
  );
  Assert.ok(await IOUtils.exists(path), "The destination folder was created.");

  await IOUtils.remove(DEFAULT_FOLDER, { recursive: true });
  await IOUtils.remove(UNWRITABLE_PARENT, { recursive: true });
  sandbox.restore();
});

/**
 * Tests that we fall back to the Home folder if the configured path AND the
 * DEFAULT_PARENT_DIR_PATH cannot be written to.
 */
add_task(async function test_fallback_to_home() {
  if (AppConstants.platform == "win") {
    todo_check_true(
      false,
      "Programmatically setting folder permissions does not work on " +
        "Windows, so this test is skipped."
    );
    return;
  }

  const UNWRITABLE_PARENT = PathUtils.join(gTestRoot, "UnwritableParent");
  await IOUtils.makeDirectory(UNWRITABLE_PARENT);
  // Make the folder read-only across the board. 0o444 is the chmod numeric code
  // for that.
  await IOUtils.setPermissions(UNWRITABLE_PARENT, 0o444);

  const CONFIGURED_FOLDER = PathUtils.join(
    UNWRITABLE_PARENT,
    "ImpossibleChild"
  );
  Assert.ok(
    !(await IOUtils.exists(CONFIGURED_FOLDER)),
    "Configured folder should not exist."
  );

  const DEFAULT_FOLDER = PathUtils.join(gTestRoot, "FakeDocuments");
  await IOUtils.makeDirectory(DEFAULT_FOLDER);
  await IOUtils.setPermissions(DEFAULT_FOLDER, 0o444);

  let bs = new BackupService();
  // Stub out the DEFAULT_PARENT_DIR_PATH into a folder path we control in this
  // test, so that we don't pollute this machine's actual Documents folder.
  let sandbox = sinon.createSandbox();
  sandbox
    .stub(BackupService, "DEFAULT_PARENT_DIR_PATH")
    .get(() => DEFAULT_FOLDER);

  const CONFIGURED_DESTINATION_PATH = PathUtils.join(
    CONFIGURED_FOLDER,
    BackupService.BACKUP_DIR_NAME
  );
  const EXPECTED_DESTINATION_PATH = PathUtils.join(
    gFakeHomePath,
    BackupService.BACKUP_DIR_NAME
  );
  let path = await bs.resolveArchiveDestFolderPath(CONFIGURED_DESTINATION_PATH);
  Assert.equal(
    path,
    EXPECTED_DESTINATION_PATH,
    "Got back the expected folder path."
  );
  Assert.ok(await IOUtils.exists(path), "The destination folder was created.");

  await IOUtils.remove(EXPECTED_DESTINATION_PATH, { recursive: true });
  await IOUtils.remove(DEFAULT_FOLDER, { recursive: true });
  await IOUtils.remove(UNWRITABLE_PARENT, { recursive: true });
  sandbox.restore();
});

/**
 * Tests that if we fall back to the $HOME folder and some how that doesn't
 * exist, then we reject.
 */
add_task(async function test_fallback_to_home_fail() {
  if (AppConstants.platform == "win") {
    todo_check_true(
      false,
      "Programmatically setting folder permissions does not work on " +
        "Windows, so this test is skipped."
    );
    return;
  }

  const UNWRITABLE_PARENT = PathUtils.join(gTestRoot, "UnwritableParent");
  await IOUtils.makeDirectory(UNWRITABLE_PARENT);
  // Make the folder read-only across the board. 0o444 is the chmod numeric code
  // for that.
  await IOUtils.setPermissions(UNWRITABLE_PARENT, 0o444);

  const CONFIGURED_FOLDER = PathUtils.join(
    UNWRITABLE_PARENT,
    "ImpossibleChild"
  );
  Assert.ok(
    !(await IOUtils.exists(CONFIGURED_FOLDER)),
    "Configured folder should not exist."
  );

  const DEFAULT_FOLDER = PathUtils.join(gTestRoot, "FakeDocuments");
  await IOUtils.makeDirectory(DEFAULT_FOLDER);
  await IOUtils.setPermissions(DEFAULT_FOLDER, 0o444);

  const UNWRITABLE_HOME_FOLDER = PathUtils.join(gTestRoot, "UnwritableHome");
  await IOUtils.makeDirectory(UNWRITABLE_HOME_FOLDER);
  await IOUtils.setPermissions(UNWRITABLE_HOME_FOLDER, 0o444);

  let unwritableHomeFolderFile = await IOUtils.getFile(UNWRITABLE_HOME_FOLDER);
  let dirsvc = Services.dirsvc.QueryInterface(Ci.nsIProperties);
  dirsvc.undefine(HOME_KEY);
  dirsvc.set(HOME_KEY, unwritableHomeFolderFile);

  // Stub out the DEFAULT_PARENT_DIR_PATH into a folder path we control in this
  // test, so that we don't pollute this machine's actual Documents folder.
  let sandbox = sinon.createSandbox();
  sandbox
    .stub(BackupService, "DEFAULT_PARENT_DIR_PATH")
    .get(() => DEFAULT_FOLDER);

  let bs = new BackupService();

  const CONFIGURED_DESTINATION_PATH = PathUtils.join(
    CONFIGURED_FOLDER,
    BackupService.BACKUP_DIR_NAME
  );

  await Assert.rejects(
    bs.resolveArchiveDestFolderPath(CONFIGURED_DESTINATION_PATH),
    /Could not resolve/
  );

  sandbox.restore();
  await IOUtils.remove(UNWRITABLE_HOME_FOLDER, { recursive: true });
  await IOUtils.remove(DEFAULT_FOLDER, { recursive: true });
  await IOUtils.remove(UNWRITABLE_PARENT, { recursive: true });

  dirsvc.undefine(HOME_KEY);
  dirsvc.set(HOME_KEY, gFakeHomeFile);
});
