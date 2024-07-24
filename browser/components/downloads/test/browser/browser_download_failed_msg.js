/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gDownloadDir;

add_setup(async function () {
  // Need to start the server before `httpUrl` calls.
  startServer();

  registerCleanupFunction(task_resetState);

  gDownloadDir = new FileUtils.File(await setDownloadDir());
});

add_task(async function test() {
  let list = await Downloads.getList(Downloads.PUBLIC);
  let downloadAdded = Promise.withResolvers();
  let downloadChanged = Promise.withResolvers();

  let view = {
    onDownloadAdded(download) {
      info("onDownloadAdded");
      downloadAdded.resolve(download);
    },
    onDownloadChanged(download) {
      info("onDownloadChanged");
      list.removeView(view);
      downloadChanged.resolve(download);
    },
  };
  list.addView(view);

  serveInterruptibleAsDownload();
  mustInterruptResponses();

  let listbox = document.getElementById("downloadsListBox");
  Assert.ok(listbox, "Download list box present");
  let promiseDownloadListPopulated = new Promise(resolve => {
    let observer = new MutationObserver(() => {
      observer.disconnect();
      resolve();
    });
    observer.observe(listbox, { childList: true });
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: httpUrl("interruptible.txt"),
      waitForLoad: false,
      waitForStop: true,
    },
    async function () {
      let download = await downloadAdded.promise;

      failResponses();
      await downloadChanged.promise;

      Assert.ok(!download.succeeded, "The download failed");
      Assert.ok(download.error, "The download had an error");
      registerCleanupFunction(async function () {
        await IOUtils.remove(download.target.path);
      });

      await promiseDownloadListPopulated;
      Assert.equal(listbox.childElementCount, 1, "Check number of downloads");
      Assert.ok(!listbox.getAttribute("disabled"), "Downloads list is enabled");

      // Check error messages differ, as the hover one is more detailed.
      let downloadElement = listbox.itemChildren[0];
      let normalTextElement = downloadElement.querySelector(
        ".downloadDetailsNormal"
      );
      let hoverTextElement = downloadElement.querySelector(
        ".downloadDetailsHover"
      );
      Assert.notEqual(
        hoverTextElement.value,
        normalTextElement.value,
        "Messages should differ as hover contains a more detailed error message"
      );
      Assert.equal(
        hoverTextElement.value,
        hoverTextElement.tooltipText,
        "Tooltip should be set the same as value, for long messages"
      );

      // Check content of the message in a fuzzy way, as it depends on l10n and
      // may change in the future.
      Assert.ok(
        hoverTextElement.value.includes("interruptible"),
        "Should include the name of the file"
      );
    }
  );
  await list.removeFinished();
});
