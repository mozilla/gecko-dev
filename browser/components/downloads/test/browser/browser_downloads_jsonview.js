/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL =
  "https://example.org/browser/browser/components/downloads/test/browser/cookies-json.sjs";

const { FileTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/FileTestUtils.sys.mjs"
);

add_task(async function test_save_jsonview() {
  // Create a cookie that will be valid for the given JSON Document
  Services.cookies.add(
    "example.org",
    "/",
    "cookieKey",
    "cookieValue",
    false,
    false,
    false,
    Number.MAX_SAFE_INTEGER,
    {},
    Ci.nsICookie.SAMESITE_LAX,
    Ci.nsICookie.SCHEME_HTTPS
  );

  // Set up the download
  let MockFilePicker = SpecialPowers.MockFilePicker;
  MockFilePicker.init(window.browsingContext);
  let list = await Downloads.getList(Downloads.PUBLIC);
  let downloadFinishedPromise = promiseDownloadFinished(list);
  let saveFile = FileTestUtils.getTempFile("cookies.sjs");
  if (!saveFile.exists()) {
    saveFile.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  }

  // Open the JSONview
  await BrowserTestUtils.withNewTab(TEST_URL, async _ => {
    await new Promise(resolve => {
      MockFilePicker.showCallback = function (fp) {
        saveFile.append("testfile");
        MockFilePicker.setFiles([saveFile]);
        setTimeout(() => {
          resolve(fp.defaultString);
        }, 0);
        return Ci.nsIFilePicker.returnOK;
      };

      // Download the JSONview page
      document.getElementById("Browser:SavePage").doCommand();
    });
  });

  // Wait for the download and make sure it is the right size
  let download = await downloadFinishedPromise;
  Assert.ok(download.stopped);
  Assert.ok(download.hasProgress);
  Assert.equal(download.progress, 100);
  Assert.equal(download.currentBytes, 45);
  Assert.equal(download.source.url, TEST_URL);
  Assert.equal(
    await IOUtils.readUTF8(download.target.path),
    '{"cookieHeaderValue":"cookieKey=cookieValue"}'
  );

  Assert.equal(await expectNonZeroDownloadTargetSize(download.target), 45);

  MockFilePicker.cleanup();
});
