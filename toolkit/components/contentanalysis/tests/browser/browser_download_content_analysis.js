/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let mockCA = makeMockContentAnalysis();

add_setup(async function test_setup() {
  mockCA = await mockContentAnalysisService(mockCA);
});

const DOWNLOAD_URL =
  "https://example.com/browser/toolkit/components/contentanalysis/tests/browser/file_to_download.unknownextension";

async function createTargetFileAndDownload() {
  // Create a temporary file that will be downloaded to.
  const targetFile = await IOUtils.createUniqueFile(
    PathUtils.tempDir,
    "target_download_for_content_analysis.txt",
    0o600
  );
  // Remove the file so tests can see whether it gets
  // successfully downloaded. Note that this theoretically introduces
  // a race condition since something else could come in and create
  // a temp file with the same name. This seems unlikely.
  await IOUtils.remove(targetFile, { ignoreAbsent: false });

  return await Downloads.createDownload({
    source: {
      url: DOWNLOAD_URL,
    },
    target: targetFile,
  });
}

/**
 * Waits for a download to finish.
 *
 * @param {Download} aDownload
 *        The Download object to wait upon.
 *
 * @returns {Promise}
 */
function promiseDownloadFinished(aDownload) {
  return new Promise(resolve => {
    // Wait for the download to finish.
    let onchange = function () {
      if (aDownload.succeeded || aDownload.error) {
        aDownload.onchange = null;
        resolve();
      }
    };

    // Register for the notification, but also call the function directly in
    // case the download already reached the expected progress.
    aDownload.onchange = onchange;
    onchange();
  });
}

function assertContentAnalysisDownloadRequest(request, expectedFilePath) {
  is(request.url.spec, DOWNLOAD_URL, "request has correct URL");
  is(
    request.analysisType,
    Ci.nsIContentAnalysisRequest.eFileDownloaded,
    "request has correct analysisType"
  );
  is(
    request.reason,
    Ci.nsIContentAnalysisRequest.eNormalDownload,
    "request has correct reason"
  );
  is(
    request.operationTypeForDisplay,
    Ci.nsIContentAnalysisRequest.eDownload,
    "request has correct operationTypeForDisplay"
  );
  is(request.filePath, expectedFilePath, "request filePath should match");
  ok(!request.textContent?.length, "request textContent should be empty");
  is(
    request.userActionRequestsCount,
    1,
    "request userActionRequestsCount should match"
  );
  ok(request.userActionId.length, "request userActionId should not be empty");
  is(request.printDataHandle, 0, "request printDataHandle should be 0");
  is(request.printDataSize, 0, "request printDataSize should be 0");
  ok(!!request.requestToken.length, "request requestToken should not be empty");
}

add_task(async function test_download_content_analysis_allows() {
  mockCA.setupForTest(true);
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.contentanalysis.interception_point.download.enabled", true],
    ],
  });

  const download = await createTargetFileAndDownload();
  await download.start();
  await promiseDownloadFinished(download);
  // Make sure the download succeeded.
  ok(download.succeeded, "Download should succeed");
  is(mockCA.calls.length, 1, "Content analysis should be called once");
  assertContentAnalysisDownloadRequest(mockCA.calls[0], download.target.path);
  ok(await IOUtils.exists(download.target.path), "Target file should exist");
  await IOUtils.remove(download.target.path);
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_download_content_analysis_blocks() {
  mockCA.setupForTest(false);
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.contentanalysis.interception_point.download.enabled", true],
    ],
  });

  const download = await createTargetFileAndDownload();
  info(`path is ${download.target.path}`);
  await Assert.rejects(
    download.start(),
    ex => ex instanceof Downloads.Error && ex.becauseBlockedByContentAnalysis,
    "Download should have been rejected"
  );
  info(`path is ${download.target.path}`);
  ok(!download.succeeded, "Download should not succeed");
  is(mockCA.calls.length, 1, "Content analysis should be called once");
  info(`path is ${download.target.path}`);
  assertContentAnalysisDownloadRequest(mockCA.calls[0], download.target.path);
  info(`path is ${download.target.path}`);

  ok(
    !(await IOUtils.exists(download.target.path)),
    "Target file should not exist"
  );
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_download_content_analysis_pref_defaults_to_off() {
  mockCA.setupForTest(false);
  // do not set pref, so content analysis should not be consulted
  const download = await createTargetFileAndDownload();
  await download.start();
  await promiseDownloadFinished(download);
  // Make sure the download succeeded.
  ok(download.succeeded, "Download should succeed");
  is(mockCA.calls.length, 0, "Content analysis should not be called");
  ok(await IOUtils.exists(download.target.path), "Target file should exist");
  await IOUtils.remove(download.target.path);
});
