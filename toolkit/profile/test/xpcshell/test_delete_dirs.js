/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { EXIT_CODE } = ChromeUtils.importESModule(
  "resource://gre/modules/BackgroundTasksManager.sys.mjs"
);
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

/**
 * Tests that attempts to remove the profile directories complete.
 */

add_setup(() => {
  // Make sure the profile service is initialised.
  let { didCreate } = selectStartupProfile();
  Assert.ok(didCreate, "Should have created a new profile.");
});

function wait(ms) {
  // There isn't really a way to detect that the background task has started waiting so we must rely
  // on a timeout here.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  return new Promise(resolve => setTimeout(resolve, ms));
}

function runDeletionTask(rootDir, localDir, timeout) {
  let binary = Services.dirsvc.get("XREExeF", Ci.nsIFile);
  binary.leafName = binary.leafName.replace(
    "xpcshell",
    AppConstants.MOZ_BUILD_APP == "browser" ? "firefox" : "thunderbird"
  );

  let process = Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);

  process.init(binary);
  process.noShell = true;
  process.startHidden = true;
  let args = [
    "--backgroundtask",
    "removeProfileFiles",
    rootDir.path,
    localDir.path,
    String(timeout),
  ];

  console.log("Executing", binary.path, ...args);

  return new Promise(resolve => {
    process.runwAsync(args, args.length, () => {
      resolve(process.exitValue);
    });
  });
}

// Test that in-process file deletion works.
add_task(async () => {
  let service = getProfileService();
  let profile = service.createProfile(null, "Test2");

  // Basic test that file deletion works.
  await service.removeProfileFilesByPath(profile.rootDir, profile.localDir, 0);

  Assert.ok(
    !(await IOUtils.exists(profile.rootDir.path)),
    "Should have deleted the root dir"
  );
  Assert.ok(
    !(await IOUtils.exists(profile.localDir.path)),
    "Should have deleted the local dir"
  );
});

// Tests that background task profile deletion works.
add_task(
  {
    skip_if: () => !AppConstants.MOZ_BACKGROUNDTASKS,
  },
  async () => {
    let service = getProfileService();
    let profile = service.createProfile(null, "Test3");

    let testRootFile = PathUtils.join(profile.rootDir.path, "test.txt");
    await IOUtils.writeUTF8(testRootFile, "hello");
    let testLocalFile = PathUtils.join(profile.localDir.path, "test.txt");
    await IOUtils.writeUTF8(testLocalFile, "goodbye");

    let exitCode = await runDeletionTask(profile.rootDir, profile.localDir, 0);

    Assert.equal(
      exitCode,
      EXIT_CODE.SUCCESS,
      "Process should have completed successfully"
    );
    Assert.ok(
      !(await IOUtils.exists(profile.rootDir.path)),
      "Should have deleted the root dir"
    );
    Assert.ok(
      !(await IOUtils.exists(profile.localDir.path)),
      "Should have deleted the local dir"
    );
  }
);

// Tests that the background task cannot delete a locked profile directory.
add_task(
  {
    skip_if: () => !AppConstants.MOZ_BACKGROUNDTASKS,
  },
  async () => {
    let service = getProfileService();
    let profile = service.createProfile(null, "Test4");

    let testRootFile = PathUtils.join(profile.rootDir.path, "test.txt");
    await IOUtils.writeUTF8(testRootFile, "hello");
    let testLocalFile = PathUtils.join(profile.localDir.path, "test.txt");
    await IOUtils.writeUTF8(testLocalFile, "goodbye");

    let lock = profile.lock({});

    // With no timeout the task will exit as soon as it fails to obtain the lock.
    let exitCode = await runDeletionTask(profile.rootDir, profile.localDir, 0);

    Assert.equal(
      exitCode,
      EXIT_CODE.EXCEPTION,
      "Process should not have completed successfully"
    );
    Assert.ok(
      await IOUtils.exists(testRootFile),
      "Should not have deleted the root dir"
    );
    Assert.ok(
      await IOUtils.exists(testLocalFile),
      "Should not have deleted the local dir"
    );

    lock.unlock();

    // But now it can succeed.
    exitCode = await runDeletionTask(profile.rootDir, profile.localDir, 0);

    Assert.equal(
      exitCode,
      EXIT_CODE.SUCCESS,
      "Process should have completed successfully"
    );
    Assert.ok(
      !(await IOUtils.exists(profile.rootDir.path)),
      "Should have deleted the root dir"
    );
    Assert.ok(
      !(await IOUtils.exists(profile.localDir.path)),
      "Should have deleted the local dir"
    );
  }
);

// Tests that the background task can wait for the profile lock.
add_task(
  {
    skip_if: () => !AppConstants.MOZ_BACKGROUNDTASKS,
  },
  async () => {
    let service = getProfileService();
    let profile = service.createProfile(null, "Test5");

    let testRootFile = PathUtils.join(profile.rootDir.path, "test.txt");
    await IOUtils.writeUTF8(testRootFile, "hello");
    let testLocalFile = PathUtils.join(profile.localDir.path, "test.txt");
    await IOUtils.writeUTF8(testLocalFile, "goodbye");

    let lock = profile.lock({});

    let deletionPromise = runDeletionTask(
      profile.rootDir,
      profile.localDir,
      20
    );

    // Wait a few seconds to allow the background task to start running. Allow
    // longer for debug builds.
    await wait(AppConstants.DEBUG ? 4000 : 1000);

    Assert.ok(
      await IOUtils.exists(testRootFile),
      "Should not have deleted the root dir"
    );
    Assert.ok(
      await IOUtils.exists(testLocalFile),
      "Should not have deleted the local dir"
    );

    // Now unlock and the deletion should complete.
    lock.unlock();
    let exitCode = await deletionPromise;

    Assert.equal(
      exitCode,
      EXIT_CODE.SUCCESS,
      "Process should have completed successfully"
    );
    Assert.ok(
      !(await IOUtils.exists(profile.rootDir.path)),
      "Should have deleted the root dir"
    );
    Assert.ok(
      !(await IOUtils.exists(profile.localDir.path)),
      "Should have deleted the local dir"
    );
  }
);
