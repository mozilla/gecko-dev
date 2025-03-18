/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var MockFilePicker = SpecialPowers.MockFilePicker;

/**
 * TestCase for bug 564387
 * <https://bugzilla.mozilla.org/show_bug.cgi?id=564387>
 */
async function testSaveVideo(isUsingHeader = true) {
  MockFilePicker.init(window.browsingContext);
  var fileName;

  let loadPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  BrowserTestUtils.startLoadingURIString(
    gBrowser,
    "http://mochi.test:8888/browser/browser/base/content/test/general/web_video.html"
  );
  await loadPromise;

  let popupShownPromise = BrowserTestUtils.waitForEvent(document, "popupshown");

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#video1",
    { type: "contextmenu", button: 2 },
    gBrowser.selectedBrowser
  );
  info("context menu click on video1");

  await popupShownPromise;

  info("context menu opened on video1");

  // Create the folder the video will be saved into.
  var destDir = createTemporarySaveDirectory();
  var destFile = destDir.clone();

  MockFilePicker.displayDirectory = destDir;
  MockFilePicker.showCallback = function (fp) {
    fileName = fp.defaultString;
    destFile.append(fileName);
    MockFilePicker.setFiles([destFile]);
    MockFilePicker.filterIndex = 1; // kSaveAsType_URL
  };

  let transferCompletePromise = new Promise(resolve => {
    function onTransferComplete(downloadSuccess) {
      ok(
        downloadSuccess,
        "Video file should have been downloaded successfully"
      );

      if (isUsingHeader) {
        is(
          fileName,
          "web-video1-expectedName.webm",
          "Video file name is correctly retrieved from Content-Disposition http header"
        );
      }
      resolve();
    }

    mockTransferCallback = onTransferComplete;
    mockTransferRegisterer.register();
  });

  // Wrap in try...finally to ensure we clean up mocks and temp files.
  // We can't use registerCleanupFunction easily because we need to clean up
  // inside each task as well.
  try {
    // Select "Save Video As" option from context menu
    var saveVideoCommand = document.getElementById("context-savevideo");
    saveVideoCommand.doCommand();
    info("context-savevideo command executed");

    let contextMenu = document.getElementById("contentAreaContextMenu");
    let popupHiddenPromise = BrowserTestUtils.waitForEvent(
      contextMenu,
      "popuphidden"
    );
    contextMenu.hidePopup();
    await popupHiddenPromise;

    await transferCompletePromise;
  } finally {
    mockTransferRegisterer.unregister();
    MockFilePicker.cleanup();
    destDir.remove(true);
  }
}

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/content/tests/browser/common/mockTransfer.js",
  this
);

function createTemporarySaveDirectory() {
  var saveDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  saveDir.append("testsavedir");
  if (!saveDir.exists()) {
    saveDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  }
  return saveDir;
}

add_task(async function test_save_video_normal() {
  return testSaveVideo();
});

/**
 * Check that saving the file also works if the initial request times out.
 */
add_task(async function test_save_video_timed_out_request() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.download.saveLinkAsFilenameTimeout", 0]],
  });
  /*
   * Note that this cannot rely on the header (we're deliberately trying to
   * test the case where we don't get the headers), but also cannot
   * assume that it is never present. That is, if network bits are very very
   * fast and/or our cancellation timeout fires late, we might be testing
   * the same thing as the previous test. So we just don't bother asserting
   * anything about the filename coming from the header - we just want to
   * make sure that the file is still saved in this case.
   */
  await testSaveVideo(/* isUsingHeader */ false);
  await SpecialPowers.popPrefEnv();
});
