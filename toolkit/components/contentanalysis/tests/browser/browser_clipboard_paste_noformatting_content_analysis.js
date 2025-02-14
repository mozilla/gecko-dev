/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let mockCA = makeMockContentAnalysis();

add_setup(async function test_setup() {
  mockCA = await mockContentAnalysisService(mockCA);
});

const PAGE_URL =
  "https://example.com/browser/toolkit/components/contentanalysis/tests/browser/clipboard_paste_noformatting.html";
async function testClipboardPasteNoFormatting(allowPaste) {
  mockCA.setupForTest(allowPaste);

  const trans = Cc["@mozilla.org/widget/transferable;1"].createInstance(
    Ci.nsITransferable
  );
  trans.init(null);
  const CLIPBOARD_TEXT_STRING = "Some text";
  const CLIPBOARD_HTML_STRING = "<b>Some HTML</b>";
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

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE_URL);
  let browser = tab.linkedBrowser;
  let result = await SpecialPowers.spawn(browser, [allowPaste], allowPaste => {
    return new Promise(resolve => {
      content.document.addEventListener("testresult", event => {
        resolve(event.detail.result);
      });
      content.document.getElementById("pasteAllowed").checked = allowPaste;
      content.document.dispatchEvent(new content.CustomEvent("teststart", {}));
    });
  });
  is(result, true, "Got unexpected result from page");

  // Since we're only pasting plain text we should only need to do one
  // call.
  is(mockCA.calls.length, 1, "Correct number of calls to Content Analysis");
  assertContentAnalysisRequest(
    mockCA.calls[0],
    CLIPBOARD_TEXT_STRING,
    mockCA.calls[0].userActionId,
    1
  );

  BrowserTestUtils.removeTab(tab);
}

function assertContentAnalysisRequest(
  request,
  expectedText,
  expectedUserActionId,
  expectedRequestsCount
) {
  is(request.url.spec, PAGE_URL, "request has correct URL");
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
  if (expectedText !== undefined) {
    is(request.textContent, expectedText, "request textContent should match");
  }
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

add_task(
  async function testClipboardPasteNoFormattingWithContentAnalysisAllow() {
    // Make sure this works even if we're analyzing all clipboard formats
    await SpecialPowers.pushPrefEnv({
      set: [
        [
          "browser.contentanalysis.interception_point.clipboard.plain_text_only",
          false,
        ],
      ],
    });
    await testClipboardPasteNoFormatting(true);
    await SpecialPowers.popPrefEnv();
  }
);

add_task(
  async function testClipboardPasteNoFormattingWithContentAnalysisBlock() {
    // Make sure this works even if we're analyzing all clipboard formats
    await SpecialPowers.pushPrefEnv({
      set: [
        [
          "browser.contentanalysis.interception_point.clipboard.plain_text_only",
          false,
        ],
      ],
    });
    await testClipboardPasteNoFormatting(false);
    await SpecialPowers.popPrefEnv();
  }
);
