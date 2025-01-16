/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { Downloads } = ChromeUtils.importESModule(
  "resource://gre/modules/Downloads.sys.mjs"
);

const TEST_DATA = "This is 31 bytes of sample data";
const TOTAL_LEN = 31;
const PARTIAL_LEN = 15;

const server = createHttpServer();

const port = server.identity.primaryPort;

const url = `http://localhost:${port}/file_download.txt`;

let requestCount = 0;
server.registerPathHandler("/file_download.txt", async (request, response) => {
  response.setHeader("Last-Modified", "Thu, 1 Jan 2009 00:00:00 GMT", false);
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Accept-Ranges", "bytes", false);

  if (!request.hasHeader("Range")) {
    equal(++requestCount, 1, "First request should expect a full response");
    response.processAsync();
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Length", "" + TOTAL_LEN, false);
    let downloadPromise = waitForDownload();
    response.write(TEST_DATA.slice(0, PARTIAL_LEN));
    let download = await downloadPromise;
    await waitForCreatedPartFile(download);
    response.finish();
  } else {
    // When downloads.resume() is called for the incomplete download
    // from above, we should receive a Range request.
    equal(++requestCount, 2, "Second request should be a Range request");
    equal(request.getHeader("Range"), `bytes=${PARTIAL_LEN}-`, "Range ok");
    response.setStatusLine(request.httpVersion, 206, "Partial Content");
    response.setHeader(
      "Content-Range",
      `${PARTIAL_LEN}-${TOTAL_LEN - 1}/${TOTAL_LEN}`,
      false
    );
    let data = TEST_DATA.slice(PARTIAL_LEN);
    response.setHeader("Content-Length", "" + data.length, false);
    response.write(data);
  }
});

async function waitForDownload() {
  let list = await Downloads.getList(Downloads.ALL);

  return new Promise(resolve => {
    const view = {
      onDownloadChanged(download) {
        if (
          download.source.url == url &&
          download.currentBytes == PARTIAL_LEN
        ) {
          list.removeView(view);
          resolve(download);
        }
      },
    };
    list.addView(view);
  });
}

async function waitForCreatedPartFile(download) {
  const partFilePath = download.target.partFilePath;

  info(`Wait for ${partFilePath} to be created`);
  let lastError;
  await TestUtils.waitForCondition(
    () =>
      IOUtils.exists(partFilePath).catch(err => {
        lastError = err;
        return false;
      }),
    `Wait for the ${partFilePath} to exists before continuing the test`
  ).catch(err => {
    if (lastError) {
      throw lastError;
    }
    throw err;
  });
}

function backgroundScript() {
  let dlid = 0;
  let expectedState;
  browser.test.onMessage.addListener(async msg => {
    try {
      if (msg.resume) {
        expectedState = "complete";
        await browser.downloads.resume(dlid);
      } else {
        expectedState = "interrupted";
        dlid = await browser.downloads.download({ url: msg.url });
      }
    } catch (err) {
      browser.test.fail(`Unexpected error in test.onMessage: ${err}`);
    }
  });
  browser.downloads.onChanged.addListener(({ id, state }) => {
    if (dlid !== id || !state || state.current !== expectedState) {
      return;
    }

    browser.downloads.search({ id }).then(([download]) => {
      browser.test.sendMessage(expectedState, download);
    });
  });

  browser.test.sendMessage("ready");
}

async function clearDownloads() {
  let list = await Downloads.getList(Downloads.ALL);
  let downloads = await list.getAll();

  await Promise.all(
    downloads.map(async download => {
      await download.finalize(true);
      list.remove(download);
    })
  );

  return downloads;
}

add_setup(() => {
  const nsIFile = Ci.nsIFile;
  let downloadDir = FileUtils.getDir("TmpD", ["downloads"]);
  downloadDir.createUnique(nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  info(`downloadDir ${downloadDir.path}`);

  Services.prefs.setIntPref("browser.download.folderList", 2);
  Services.prefs.setComplexValue("browser.download.dir", nsIFile, downloadDir);

  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("browser.download.folderList");
    Services.prefs.clearUserPref("browser.download.dir");
    await clearDownloads();
    downloadDir.remove(true);
  });
});

add_task(async function test_resume_download_after_network_error() {
  let extension = ExtensionTestUtils.loadExtension({
    background: backgroundScript,
    manifest: {
      permissions: ["downloads"],
    },
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  extension.sendMessage({ url });

  let download = await extension.awaitMessage("interrupted");
  equal(download.error, "NETWORK_FAILED", "download() failed");
  equal(download.state, "interrupted", "download.state is correct");
  equal(download.paused, false, "download.paused is correct");
  equal(
    download.estimatedEndTime,
    undefined,
    "download.estimatedEndTime is correct"
  );
  equal(download.canResume, true, "download.canResume is correct");
  equal(
    download.bytesReceived,
    PARTIAL_LEN,
    "download.bytesReceived is correct"
  );
  equal(download.totalBytes, TOTAL_LEN, "download.totalBytes is correct");
  equal(download.exists, false, "download.exists is correct");

  extension.sendMessage({ resume: true });

  download = await extension.awaitMessage("complete");
  equal(download.state, "complete", "download.state is correct");
  equal(download.paused, false, "download.paused is correct");
  equal(
    download.estimatedEndTime,
    undefined,
    "download.estimatedEndTime is correct"
  );
  equal(download.canResume, false, "download.canResume is correct");
  equal(download.error, null, "download.error is correct");
  equal(download.bytesReceived, TOTAL_LEN, "download.bytesReceived is correct");
  equal(download.totalBytes, TOTAL_LEN, "download.totalBytes is correct");
  equal(download.exists, true, "download.exists is correct");

  await extension.unload();
});
