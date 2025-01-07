/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let mockCA = makeMockContentAnalysis();

add_setup(async function test_setup() {
  mockCA = await mockContentAnalysisService(mockCA);
});

const PAGE_URL =
  "https://example.com/browser/toolkit/components/contentanalysis/tests/browser/clipboard_paste_changingclipboardexternal.html";
const CLIPBOARD_TEXT_STRING_ORIGINAL = "Original text";
const CLIPBOARD_TEXT_STRING_NEW = "New text";

// Test that if the clipboard contents change externally (i.e. the user
// does a copy from some other application) while Content Analysis is ongoing,
// the new contents are ignored and the original contents of the clipboard
// are put in the DOM element.
async function testClipboardPasteWithContentAnalysis(shouldAllow) {
  mockCA.setupForTest(shouldAllow, true);

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE_URL);
  let browser = tab.linkedBrowser;

  await SpecialPowers.spawn(browser, [shouldAllow], async shouldAllow => {
    content.document.getElementById("pasteAllowed").checked = shouldAllow;
  });
  await testPasteWithElementId("testDiv", browser, shouldAllow);
  await testPasteWithElementId("testInput", browser, shouldAllow);

  BrowserTestUtils.removeTab(tab);
}

add_task(async function testClipboardPasteWithContentAnalysisAllow() {
  await testClipboardPasteWithContentAnalysis(true);
});

add_task(async function testClipboardPasteWithContentAnalysisBlock() {
  await testClipboardPasteWithContentAnalysis(false);
});

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

async function testPasteWithElementId(elementId, browser, shouldAllow) {
  setClipboardData(CLIPBOARD_TEXT_STRING_ORIGINAL);
  let resultPromise = SpecialPowers.spawn(browser, [], () => {
    return new Promise(resolve => {
      content.document.addEventListener(
        "testresult",
        event => {
          resolve(event.detail.result);
        },
        { once: true }
      );
    });
  });

  // Paste into content
  await setElementValue(browser, elementId, "");
  await SpecialPowers.spawn(browser, [elementId], async elementId => {
    content.document.getElementById(elementId).focus();
  });
  let doPastePromise = BrowserTestUtils.synthesizeKey(
    "v",
    { accelKey: true },
    browser
  );
  // While scan is ongoing, set clipboard to new value
  await new Promise(res => {
    mockCA.eventTarget.addEventListener(
      "inAnalyzeContentRequest",
      () => {
        res();
      },
      { once: true }
    );
  });
  setClipboardData(CLIPBOARD_TEXT_STRING_NEW);
  mockCA.waitForEvent = false;
  mockCA.eventTarget.dispatchEvent(
    new CustomEvent("returnContentAnalysisResponse")
  );
  await doPastePromise;
  let result = await resultPromise;
  is(result, undefined, "Got unexpected result from page");

  is(mockCA.calls.length, 1, "Correct number of calls to Content Analysis");
  assertContentAnalysisRequest(mockCA.calls[0], CLIPBOARD_TEXT_STRING_ORIGINAL);
  mockCA.clearCalls();
  let value = await getElementValue(browser, elementId);
  // Since the clipboard was set externally during the content analysis call,
  // it should not be set in the HTML element (and the original value
  // should be used). This is to prevent cases where the user pastes some
  // content in the page, content analysis is taking a while, and they move
  // on to do something else on their machine and copy some sensitive data
  // like a password for use elsewhere - in this case we don't want the page
  // to see that sensitive data.
  is(
    value,
    shouldAllow ? CLIPBOARD_TEXT_STRING_ORIGINAL : "",
    "element has correct value"
  );
  mockCA.waitForEvent = true;
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
  is(request.filePath, "", "request filePath should match");
  is(request.textContent, expectedText, "request textContent should match");
  is(request.printDataHandle, 0, "request printDataHandle should not be 0");
  is(request.printDataSize, 0, "request printDataSize should not be 0");
  ok(!!request.requestToken.length, "request requestToken should not be empty");
}

async function getElementValue(browser, elementId) {
  return await SpecialPowers.spawn(browser, [elementId], async elementId => {
    let element = content.document.getElementById(elementId);
    return element.value ?? element.innerText;
  });
}

async function setElementValue(browser, elementId, value) {
  await SpecialPowers.spawn(
    browser,
    [elementId, value],
    async (elementId, value) => {
      let element = content.document.getElementById(elementId);
      if (element.hasOwnProperty("value")) {
        element.value = value;
      } else {
        element.innerText = value;
      }
    }
  );
}
