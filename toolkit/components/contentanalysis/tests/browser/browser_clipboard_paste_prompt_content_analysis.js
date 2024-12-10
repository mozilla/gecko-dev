/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { PromptTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromptTestUtils.sys.mjs"
);

let mockCA = makeMockContentAnalysis();

add_setup(async function test_setup() {
  mockCA = await mockContentAnalysisService(mockCA);
});

// Using an external page so the test can checks that the URL matches in the nsIContentAnalysisRequest
const PAGE_URL =
  "https://example.com/browser/toolkit/components/contentanalysis/tests/browser/clipboard_paste_prompt.html";
const CLIPBOARD_TEXT_STRING = "Just some text";
const TEST_MODES = Object.freeze({
  ALLOW: {
    caAllow: true,
    shouldPaste: true,
    shouldRunCA: true,
  },
  BLOCK: {
    caAllow: false,
    shouldPaste: false,
    shouldRunCA: true,
  },
  PREFOFF: {
    caAllow: false,
    shouldPaste: true,
    shouldRunCA: false,
  },
});

async function testClipboardPaste(testMode) {
  mockCA.setupForTest(testMode.caAllow);

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE_URL);
  let browser = tab.linkedBrowser;

  let promptPromise = SpecialPowers.spawn(browser, [], async () => {
    return content.prompt();
  });

  let prompt = await PromptTestUtils.waitForPrompt(browser, {
    modalType: Services.prompt.MODAL_TYPE_CONTENT,
  });
  // Paste text into prompt() in content
  let pastePromise = new Promise(resolve => {
    prompt.ui.loginTextbox.addEventListener(
      "paste",
      () => {
        // Since mockCA uses setTimeout before invoking the callback,
        // do it here too
        setTimeout(() => {
          resolve();
        }, 0);
      },
      { once: true }
    );
  });
  // Paste text into prompt()
  setClipboardData(CLIPBOARD_TEXT_STRING);
  prompt.ui.loginTextbox.focus();
  await EventUtils.synthesizeKey("v", { accelKey: true });

  await pastePromise;

  // Close the prompt
  await PromptTestUtils.handlePrompt(prompt);

  let result = await promptPromise;
  is(
    result,
    testMode.shouldPaste ? CLIPBOARD_TEXT_STRING : "",
    "prompt has correct value"
  );
  is(
    mockCA.calls.length,
    testMode.shouldRunCA ? 1 : 0,
    "Correct number of calls to Content Analysis"
  );
  if (testMode.shouldRunCA) {
    assertContentAnalysisRequest(mockCA.calls[0], CLIPBOARD_TEXT_STRING);
  }
  is(
    mockCA.browsingContextsForURIs.length,
    testMode.shouldRunCA ? 1 : 0,
    "Correct number of calls to getURIForBrowsingContext()"
  );

  BrowserTestUtils.removeTab(tab);
}

function assertContentAnalysisRequest(request, expectedText) {
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
  is(request.filePath, null, "request filePath should match");
  is(request.textContent, expectedText, "request textContent should match");
  is(request.printDataHandle, 0, "request printDataHandle should not be 0");
  is(request.printDataSize, 0, "request printDataSize should not be 0");
  ok(!!request.requestToken.length, "request requestToken should not be empty");
}

function setClipboardData(clipboardString) {
  const trans = Cc["@mozilla.org/widget/transferable;1"].createInstance(
    Ci.nsITransferable
  );
  trans.init(null);
  trans.addDataFlavor("text/plain");
  const str = Cc["@mozilla.org/supports-string;1"].createInstance(
    Ci.nsISupportsString
  );
  str.data = clipboardString;
  trans.setTransferData("text/plain", str);

  // Write to clipboard.
  Services.clipboard.setData(trans, null, Ci.nsIClipboard.kGlobalClipboard);
}

add_task(async function testClipboardPasteWithContentAnalysisAllow() {
  await testClipboardPaste(TEST_MODES.ALLOW);
});

add_task(async function testClipboardPasteWithContentAnalysisBlock() {
  await testClipboardPaste(TEST_MODES.BLOCK);
});

add_task(async function testClipboardPasteWithContentAnalysisBlockButPrefOff() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.contentanalysis.interception_point.clipboard.enabled", false],
    ],
  });
  await testClipboardPaste(TEST_MODES.PREFOFF);
  await SpecialPowers.popPrefEnv();
});
