/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Checks in default conditions a downlod succeeds even if the temp folder is
 * inaccessible (access denied).
 */

let gDownloadDir;

add_setup(async function () {
  // Need to start the server before `httpUrl` calls.
  startServer();

  registerCleanupFunction(task_resetState);

  gDownloadDir = new FileUtils.File(await setDownloadDir());

  // Ensure we're not starting downloads in tmp, otherwise this test doesn't
  // make sense.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.download.start_downloads_in_tmp_dir", false]],
  });

  // Replace the temp folder with an inaccessible one.
  const originalTmp = Services.dirsvc.get("TmpD", Ci.nsIFile);
  const path = PathUtils.join(PathUtils.profileDir, "rotemp");
  await IOUtils.makeDirectory(path);
  await IOUtils.setPermissions(path, 0o444);

  registerCleanupFunction(async () => {
    Services.dirsvc.undefine("TmpD");
    Services.dirsvc.set("TmpD", originalTmp);
    await IOUtils.setPermissions(path, 0o777);
    await IOUtils.remove(path, { recursive: true });
  });

  Services.dirsvc.undefine("TmpD");
  Services.dirsvc.set("TmpD", await IOUtils.getFile(path));
});

add_task(async function test() {
  let list = await Downloads.getList(Downloads.PUBLIC);
  let downloadStarted = new Promise(resolve => {
    let view = {
      onDownloadAdded(download) {
        info("onDownloadAdded");
        list.removeView(view);
        resolve(download);
      },
    };
    list.addView(view);
  });

  serveInterruptibleAsDownload();
  mustInterruptResponses();

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: httpUrl("interruptible.txt"),
      waitForLoad: false,
      waitForStop: true,
    },
    async function () {
      let download = await downloadStarted;
      Assert.equal(
        PathUtils.parent(download.target.path),
        gDownloadDir.path,
        "Should have put final file in the downloads dir."
      );
      continueResponses();
      await download.whenSucceeded();
      Assert.ok(download.succeeded, "The download succeeded");
      Assert.ok(!download.error, "The download has no error");
      await IOUtils.remove(download.target.path);
    }
  );
  await list.removeFinished();
});
