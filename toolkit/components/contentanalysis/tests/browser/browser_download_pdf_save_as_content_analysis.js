/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/pdfjs/test/head.js",
  this
);

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/content/tests/browser/common/mockTransfer.js",
  this
);

let MockFilePicker = SpecialPowers.MockFilePicker;
let mockCA = makeMockContentAnalysis();
let tempDir = createTemporarySaveDirectory();

add_setup(async function test_setup() {
  mockCA = await mockContentAnalysisService(mockCA);
  MockFilePicker.init(window.browsingContext);
  MockFilePicker.returnValue = MockFilePicker.returnOK;
  MockFilePicker.displayDirectory = tempDir;

  registerCleanupFunction(async function () {
    MockFilePicker.cleanup();
    await cleanupDownloads();
    tempDir.remove(true);
  });
});

const TEST_PDF_URL =
  "https://example.com/browser/toolkit/components/contentanalysis/tests/browser/file_pdf.pdf";

function createPromiseForFilePicker() {
  return new Promise(resolve => {
    MockFilePicker.showCallback = fp => {
      let destFile = tempDir.clone();
      destFile.append(fp.defaultString);
      if (destFile.exists()) {
        destFile.remove(false);
      }
      MockFilePicker.setFiles([destFile]);
      MockFilePicker.filterIndex = 0; // kSaveAsType_Complete
      resolve();
    };
  });
}

async function promiseDownloadFinished(list) {
  return new Promise(resolve => {
    list.addView({
      onDownloadChanged(download) {
        download.launchWhenSucceeded = false;
        if (download.succeeded || download.error) {
          list.removeView(this);
          resolve(download);
        }
      },
    });
  });
}

function assertContentAnalysisSaveAsRequest(request, expectedFilePath) {
  is(request.url.spec, TEST_PDF_URL, "request has correct URL");
  is(
    request.analysisType,
    Ci.nsIContentAnalysisRequest.eFileDownloaded,
    "request has correct analysisType"
  );
  is(
    request.reason,
    Ci.nsIContentAnalysisRequest.eSaveAsDownload,
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

async function testSaveAsPDF(allow) {
  mockCA.setupForTest(allow);
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.contentanalysis.interception_point.download.enabled", true],
    ],
  });
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (browser) {
      await waitForPdfJS(browser, TEST_PDF_URL);

      let downloadList = await Downloads.getList(Downloads.PUBLIC);
      let filePickerShown = createPromiseForFilePicker();
      let downloadFinishedPromise = promiseDownloadFinished(downloadList);

      info("Clicking on the download button...");
      await SpecialPowers.spawn(browser, [], () => {
        content.document.querySelector("#downloadButton").click();
      });
      info("Waiting for a filename to be picked from the file picker");
      await filePickerShown;
      let download = await downloadFinishedPromise;

      is(mockCA.calls.length, 1, "Content Analysis called once");
      assertContentAnalysisSaveAsRequest(mockCA.calls[0], download.target.path);
      is(
        await IOUtils.exists(download.target.path),
        allow,
        "Target file existence"
      );
      if (allow) {
        ok(!download.error, "Download should not have an error");
      } else {
        ok(download.error.becauseBlocked, "Download blocked");
        ok(
          download.error.becauseBlockedByContentAnalysis,
          "Download blocked by content analysis"
        );
        is(
          download.error.reputationCheckVerdict,
          "Malware",
          "Malware verdict expected"
        );
      }

      await waitForPdfJSClose(browser);
    }
  );
  await SpecialPowers.popPrefEnv();
}

add_task(async function testSaveAsPDFAllow() {
  await testSaveAsPDF(true);
});

add_task(async function testSaveAsPDFBlock() {
  await testSaveAsPDF(false);
});
