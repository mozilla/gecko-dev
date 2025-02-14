/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let mockCA = makeMockContentAnalysis();

add_setup(async function test_setup() {
  mockCA = await mockContentAnalysisService(mockCA);
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.events.asyncClipboard.readText", true],
      // This pref turns off the "Paste" popup
      ["dom.events.testing.asyncClipboard", true],
    ],
  });
});

const PAGE_URL =
  "https://example.com/browser/toolkit/components/contentanalysis/tests/browser/clipboard_read_async.html";
const CLIPBOARD_TEXT_STRING = "Some plain text";
const CLIPBOARD_HTML_STRING = "<b>Some HTML</b>";
const TEST_MODES = Object.freeze({
  ALLOW_PLAINTEXTONLY: {
    caAllow: true,
    caError: false,
    plainTextOnly: true,
    shouldPaste: true,
    shouldRunCA: true,
  },
  ALLOW_CHECKALLFORMATS: {
    caAllow: true,
    caError: false,
    plainTextOnly: false,
    shouldPaste: true,
    shouldRunCA: true,
  },
  BLOCK_PLAINTEXTONLY: {
    caAllow: false,
    caError: false,
    plainTextOnly: true,
    shouldPaste: false,
    shouldRunCA: true,
  },
  BLOCK_CHECKALLFORMATS: {
    caAllow: false,
    caError: false,
    plainTextOnly: false,
    shouldPaste: false,
    shouldRunCA: true,
  },
  ERROR: {
    caAllow: false,
    caError: true,
    plainTextOnly: false,
    shouldPaste: false,
    shouldRunCA: true,
  },
  PREFOFF: {
    caAllow: false,
    caError: false,
    plainTextOnly: false,
    shouldPaste: true,
    shouldRunCA: false,
  },
});

async function testClipboardReadAsync(testMode) {
  if (testMode.caError) {
    mockCA.setupForTestWithError(Cr.NS_ERROR_NOT_AVAILABLE);
  } else {
    mockCA.setupForTest(testMode.caAllow);
  }

  setClipboardData();

  if (testMode.caError) {
    // This test throws a number of exceptions, so tell the framework this is OK.
    // If an exception is thrown we won't get the right response from setDataAndStartTest()
    // so this should be safe to do.
    ignoreAllUncaughtExceptions();
  }
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE_URL);
  let browser = tab.linkedBrowser;
  {
    let result = await setDataAndStartTest(
      browser,
      testMode.shouldPaste,
      testMode.plainTextOnly,
      "read",
      testMode.caError
    );
    is(result, true, "Got unexpected result from page for read()");

    let expectedCACalls = 0;
    if (testMode.shouldRunCA) {
      expectedCACalls = testMode.plainTextOnly ? 1 : 2;
    }
    is(
      mockCA.calls.length,
      expectedCACalls,
      "Correct number of calls to Content Analysis for read()"
    );
    if (testMode.shouldRunCA && !testMode.caError) {
      if (!testMode.plainTextOnly) {
        // On Windows, widget adds extra data into HTML clipboard.
        let expectedHtml = navigator.platform.includes("Win")
          ? `<html><body>\n<!--StartFragment-->${CLIPBOARD_HTML_STRING}<!--EndFragment-->\n</body>\n</html>`
          : CLIPBOARD_HTML_STRING;

        // Once bug 1938618 is fixed these should have the same userActionId and the
        // expectedRequestsCount should be 2.
        assertContentAnalysisRequest(
          mockCA.calls[0],
          expectedHtml,
          mockCA.calls[0].userActionId,
          1
        );
      }
      let entry = mockCA.calls[testMode.plainTextOnly ? 0 : 1];
      assertContentAnalysisRequest(
        entry,
        CLIPBOARD_TEXT_STRING,
        entry.userActionId,
        1
      );
    }
    mockCA.clearCalls();
  }

  {
    let result = await setDataAndStartTest(
      browser,
      testMode.shouldPaste,
      testMode.plainTextOnly,
      "readText",
      testMode.caError
    );
    is(result, true, "Got unexpected result from page for readText()");

    is(
      mockCA.calls.length,
      testMode.shouldRunCA ? 1 : 0,
      "Correct number of calls to Content Analysis for read()"
    );
    if (testMode.shouldRunCA && !testMode.caError) {
      assertContentAnalysisRequest(
        mockCA.calls[0],
        CLIPBOARD_TEXT_STRING,
        mockCA.calls[0].userActionId,
        1
      );
    }
    mockCA.clearCalls();
  }

  BrowserTestUtils.removeTab(tab);
}

function setDataAndStartTest(
  browser,
  allowPaste,
  plainTextOnly,
  testType,
  shouldError = false
) {
  return SpecialPowers.spawn(
    browser,
    [allowPaste, plainTextOnly, testType, shouldError],
    (allowPaste, plainTextOnly, testType, shouldError) => {
      return new Promise(resolve => {
        content.document.addEventListener(
          "testresult",
          event => {
            resolve(event.detail.result);
          },
          { once: true }
        );
        content.document.getElementById("pasteAllowed").checked = allowPaste;
        content.document.getElementById("plainTextOnly").checked =
          plainTextOnly;
        content.document.getElementById("contentAnalysisReturnsError").checked =
          shouldError;
        content.document.dispatchEvent(
          new content.CustomEvent("teststart", {
            detail: Cu.cloneInto({ testType }, content),
          })
        );
      });
    }
  );
}

function assertContentAnalysisRequest(
  request,
  expectedText,
  expectedUserActionId,
  expectedRequestsCount
) {
  is(
    (request.url && request.url.spec) ||
      request.windowGlobalParent.documentPrincipal.URI.spec,
    PAGE_URL,
    "request has correct URL"
  );
  is(
    request.analysisType,
    Ci.nsIContentAnalysisRequest.eBulkDataEntry,
    "request has correct analysisType"
  );
  is(
    request.reason,
    Ci.nsIContentAnalysisRequest.eClipboardPaste,
    "request has correct reason"
  );
  is(
    request.operationTypeForDisplay,
    Ci.nsIContentAnalysisRequest.eClipboard,
    "request has correct operationTypeForDisplay"
  );
  is(request.filePath, "", "request filePath should match");
  is(request.textContent, expectedText, "request textContent should match");
  is(
    request.userActionRequestsCount,
    expectedRequestsCount,
    "request userActionRequestsCount should match"
  );
  is(
    request.userActionId,
    expectedUserActionId,
    "request userActionId should match"
  );
  ok(request.userActionId.length, "request userActionId should not be empty");
  is(request.printDataHandle, 0, "request printDataHandle should not be 0");
  is(request.printDataSize, 0, "request printDataSize should not be 0");
  ok(!!request.requestToken.length, "request requestToken should not be empty");
}

function setClipboardData() {
  const trans = Cc["@mozilla.org/widget/transferable;1"].createInstance(
    Ci.nsITransferable
  );
  trans.init(null);
  {
    trans.addDataFlavor("text/plain");
    const str = Cc["@mozilla.org/supports-string;1"].createInstance(
      Ci.nsISupportsString
    );
    str.data = CLIPBOARD_TEXT_STRING;
    trans.setTransferData("text/plain", str);
  }
  {
    trans.addDataFlavor("text/html");
    const str = Cc["@mozilla.org/supports-string;1"].createInstance(
      Ci.nsISupportsString
    );
    str.data = CLIPBOARD_HTML_STRING;
    trans.setTransferData("text/html", str);
  }

  // Write to clipboard.
  Services.clipboard.setData(trans, null, Ci.nsIClipboard.kGlobalClipboard);
}

add_task(
  async function testClipboardReadAsyncWithContentAnalysisAllowPlainTextOnly() {
    await SpecialPowers.pushPrefEnv({
      set: [
        [
          "browser.contentanalysis.interception_point.clipboard.plain_text_only",
          true,
        ],
      ],
    });
    await testClipboardReadAsync(TEST_MODES.ALLOW_PLAINTEXTONLY);
    await SpecialPowers.popPrefEnv();
  }
);

add_task(
  async function testClipboardReadAsyncWithContentAnalysisAllowCheckAllFormats() {
    await SpecialPowers.pushPrefEnv({
      set: [
        [
          "browser.contentanalysis.interception_point.clipboard.plain_text_only",
          false,
        ],
      ],
    });
    await testClipboardReadAsync(TEST_MODES.ALLOW_CHECKALLFORMATS);
    await SpecialPowers.popPrefEnv();
  }
);

add_task(
  async function testClipboardReadAsyncWithContentAnalysisBlockPlainTextOnly() {
    await SpecialPowers.pushPrefEnv({
      set: [
        [
          "browser.contentanalysis.interception_point.clipboard.plain_text_only",
          true,
        ],
      ],
    });
    await testClipboardReadAsync(TEST_MODES.BLOCK_PLAINTEXTONLY);
    await SpecialPowers.popPrefEnv();
  }
);

add_task(
  async function testClipboardReadAsyncWithContentAnalysisBlockCheckAllFormats() {
    await SpecialPowers.pushPrefEnv({
      set: [
        [
          "browser.contentanalysis.interception_point.clipboard.plain_text_only",
          false,
        ],
      ],
    });
    await testClipboardReadAsync(TEST_MODES.BLOCK_CHECKALLFORMATS);
    await SpecialPowers.popPrefEnv();
  }
);

add_task(
  async function testClipboardReadAsyncWithContentAnalysisBlockButPrefOff() {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.contentanalysis.interception_point.clipboard.enabled", false],
      ],
    });
    await testClipboardReadAsync(TEST_MODES.PREFOFF);
    await SpecialPowers.popPrefEnv();
  }
);

add_task(async function testClipboardReadAsyncWithError() {
  await testClipboardReadAsync(TEST_MODES.ERROR);
});
