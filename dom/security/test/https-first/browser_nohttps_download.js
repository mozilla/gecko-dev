"use strict";

// Create a uri for an https site
const testPath = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);
const TEST_URI = testPath + "file_nohttps_download.html";
const EXPECTED_DOWNLOAD_URL =
  "example.com/browser/dom/security/test/https-first/file_nohttps_download.sjs";

function promisePanelOpened() {
  if (DownloadsPanel.panel && DownloadsPanel.panel.state == "open") {
    return Promise.resolve();
  }
  return BrowserTestUtils.waitForEvent(DownloadsPanel.panel, "popupshown");
}

// Test description:
// 1. Open https://example.com/...
// 2. Start download - location of download is http
// 3. https-first upgrades to https
// 4. https returns 404 => downgrade to http again
// 5. Complete download of text file
add_task(async function test_nohttps_download() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_first", true]],
  });

  // remove all previous downloads
  let downloadsList = await Downloads.getList(Downloads.PUBLIC);
  await downloadsList.removeFinished();

  let downloadsPanelPromise = promisePanelOpened();
  let downloadsPromise = Downloads.getList(Downloads.PUBLIC);
  BrowserTestUtils.startLoadingURIString(gBrowser, TEST_URI);
  // wait for downloadsPanel to open before continuing with test
  await downloadsPanelPromise;
  let downloadList = await downloadsPromise;
  is(DownloadsPanel.isPanelShowing, true, "DownloadsPanel should be open.");
  is(downloadList._downloads.length, 1, "Entry should be in downloads list.");
  let [download] = downloadList._downloads;
  // wait for download to finish (with success or error)
  await download.unblock();
  await download.start();
  is(download.contentType, "text/plain", "File contentType should be correct.");
  // ensure https-first did upgrade the scheme.
  is(
    download.source.url,
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    "http://" + EXPECTED_DOWNLOAD_URL,
    "Scheme should be http."
  );
  // ensure that downloaded is complete
  is(download.target.size, 15, "Download size is correct");
  //clean up
  info("cleaning up downloads");
  try {
    if (Services.appinfo.OS === "WINNT") {
      // We need to make the file writable to delete it on Windows.
      await IOUtils.setPermissions(download.target.path, 0o600);
    }
    await IOUtils.remove(download.target.path);
  } catch (error) {
    info("The file " + download.target.path + " is not removed, " + error);
  }

  await downloadList.remove(download);
  await download.finalize();
});
