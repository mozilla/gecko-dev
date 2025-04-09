/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let mockCA = makeMockContentAnalysis();

add_setup(async function test_setup() {
  mockCA = await mockContentAnalysisService(mockCA);
});

// Based on editor/libeditor/tests/test_paste_redirect_focus_in_paste_event_listener.html

const PAGE_URL =
  "https://example.com/browser/toolkit/components/contentanalysis/tests/browser/clipboard_paste_redirect_focus_in_paste_event_listener.html";
const CLIPBOARD_TEXT_STRING = "plain text";

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

async function testPasteWithElementId(elementId, browser) {
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
  await SpecialPowers.spawn(browser, [elementId], async elementId => {
    content.document.getElementById(elementId).focus();
  });
  await BrowserTestUtils.synthesizeKey("v", { accelKey: true }, browser);
  let result = await resultPromise;
  is(result, undefined, "Got unexpected result from page");

  is(mockCA.calls.length, 1, "Correct number of calls to Content Analysis");
  assertContentAnalysisRequest(
    mockCA.calls[0],
    CLIPBOARD_TEXT_STRING,
    mockCA.calls[0].userActionId,
    1
  );
  mockCA.clearCalls();
  //TODO
  let value = await getElementValue(browser, elementId);
  is(value, CLIPBOARD_TEXT_STRING, "element has correct value");
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

// Must be called from inside SpecialPowers.spawn()
function getElementValue(elementId) {
  let elem = content.document.getElementById(elementId);
  let tagName = elem.tagName.toLowerCase();
  if (tagName === "input" || tagName === "textarea") {
    return elem.value;
  }
  return elem.textContent;
}

// Must be called from inside SpecialPowers.spawn()
function setElementValue(elementId, value) {
  let elem = content.document.getElementById(elementId);
  let tagName = elem.tagName.toLowerCase();
  if (tagName === "input" || tagName === "textarea") {
    elem.value = value;
  } else {
    elem.innerHTML = value;
  }
}

add_task(async function testClipboardPasteWithRedirectFocus() {
  mockCA.setupForTest(true);

  setClipboardData(CLIPBOARD_TEXT_STRING);

  const transferable = SpecialPowers.Cc[
    "@mozilla.org/widget/transferable;1"
  ].createInstance(SpecialPowers.Ci.nsITransferable);
  transferable.init(
    SpecialPowers.wrap(window).docShell.QueryInterface(
      SpecialPowers.Ci.nsILoadContext
    )
  );
  const supportString = SpecialPowers.Cc[
    "@mozilla.org/supports-string;1"
  ].createInstance(SpecialPowers.Ci.nsISupportsString);
  supportString.data = "plain text";
  transferable.setTransferData("text/plain", supportString);

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE_URL);
  let browser = tab.linkedBrowser;

  for (const command of [
    "cmd_paste",
    "cmd_pasteNoFormatting",
    "cmd_pasteQuote",
    "cmd_pasteTransferable",
  ]) {
    for (const editableSelector of [
      "#src > input",
      "#src > textarea",
      "#src > div[contenteditable]",
    ]) {
      await SpecialPowers.spawn(
        browser,
        [editableSelector],
        async editableSelector => {
          const editableElement =
            content.document.querySelector(editableSelector);
          await (async () => {
            const input = content.document.querySelector("#dest > input");
            editableElement.focus();
            editableElement.addEventListener("paste", () => input.focus(), {
              once: true,
            });
          })();
        }
      );
      await BrowserTestUtils.synthesizeKey("v", { accelKey: true }, browser);
      await SpecialPowers.spawn(
        browser,
        [command, editableSelector],
        async (command, editableSelector) => {
          // Must be called from inside SpecialPowers.spawn()
          function getElementValue(elem) {
            let tagName = elem.tagName.toLowerCase();
            if (tagName === "input" || tagName === "textarea") {
              return elem.value;
            }
            return elem.textContent;
          }
          function setElementValue(elem, value) {
            let tagName = elem.tagName.toLowerCase();
            if (tagName === "input" || tagName === "textarea") {
              elem.value = value;
            } else {
              elem.innerHTML = value;
            }
          }

          const editableElement =
            content.document.querySelector(editableSelector);
          const editableElementDesc = `<${editableElement.tagName.toLocaleLowerCase()}${editableElement.hasAttribute("contenteditable") ? " contenteditable" : ""}>`;
          const input = content.document.querySelector("#dest > input");
          is(
            getElementValue(editableElement).replace(/\n/g, ""),
            "",
            `${command}: ${
              editableElementDesc
            } should not have the pasted text because focus is redirected to <input> in a "paste" event listener`
          );
          is(
            input.value.replace("> ", ""),
            "plain text",
            `${command}: new focused <input> (moved from ${
              editableElementDesc
            }) should have the pasted text`
          );

          setElementValue(editableElement, "");
          input.value = "";
        }
      );

      is(mockCA.calls.length, 1, "Correct number of calls to Content Analysis");
      assertContentAnalysisRequest(
        mockCA.calls[0],
        CLIPBOARD_TEXT_STRING,
        mockCA.calls[0].userActionId,
        1
      );
      mockCA.clearCalls();
    }
  }

  BrowserTestUtils.removeTab(tab);
});
