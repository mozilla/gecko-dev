/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let mockCA = makeMockContentAnalysis();

add_setup(async function test_setup() {
  mockCA = await mockContentAnalysisService(mockCA);
});

const PAGE_URL =
  "https://example.com/browser/toolkit/components/contentanalysis/tests/browser/clipboard_paste_chat_shortcuts.html";
const CHAT_PROVIDER_URL = "http://localhost:8080/";
const CLIPBOARD_TEXT_STRING = "Some GenAI shortcut text";

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

function assertContentAnalysisRequest(
  request,
  expectedText,
  expectedUserActionId,
  expectedRequestsCount
) {
  is(
    request.url.spec,
    CHAT_PROVIDER_URL,
    "request has correct (chat provider) URL"
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
  is(request.filePath, null, "request filePath should match");
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

add_task(async function testClipboardPasteIntoChatShortcut() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.ml.chat.shortcuts", true],
      ["browser.ml.chat.shortcuts.custom", true],
      ["browser.ml.chat.provider", CHAT_PROVIDER_URL],
    ],
  });

  mockCA.setupForTest(true);

  setClipboardData(CLIPBOARD_TEXT_STRING);

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE_URL);
  let browser = tab.linkedBrowser;

  await SimpleTest.promiseFocus(browser);
  const selectPromise = SpecialPowers.spawn(browser, [], () => {
    ContentTaskUtils.waitForCondition(() => content.getSelection());
  });
  goDoCommand("cmd_selectAll");
  await selectPromise;
  BrowserTestUtils.synthesizeMouseAtCenter(
    browser,
    { type: "mouseup" },
    browser
  );

  await TestUtils.waitForCondition(() => {
    const panelElement = document.getElementById(
      "selection-shortcut-action-panel"
    );
    return panelElement.getAttribute("panelopen") === "true";
  }, "Wait for shortcut action panel icon to show");
  const shortcuts = document.querySelector("#ai-action-button");
  const popup = document.getElementById("chat-shortcuts-options-panel");

  EventUtils.sendMouseEvent({ type: "mouseover" }, shortcuts);
  await BrowserTestUtils.waitForEvent(popup, "popupshown");

  const shortcutTextArea = document.querySelector(
    ".ask-chat-shortcuts-custom-prompt"
  );
  shortcutTextArea.focus();
  // Note that we use the EventUtils version here because we're sending this to the
  // parent process, since the GenAI overlay isn't part of the page's content.
  EventUtils.synthesizeKey("v", { accelKey: true });

  await TestUtils.waitForCondition(() => {
    return shortcutTextArea.value === CLIPBOARD_TEXT_STRING;
  }, "Wait for chat shortcuts textarea to get pasted text");

  is(mockCA.calls.length, 1, "Correct number of calls to Content Analysis");
  assertContentAnalysisRequest(
    mockCA.calls[0],
    CLIPBOARD_TEXT_STRING,
    mockCA.calls[0].userActionId,
    1
  );
  mockCA.clearCalls();

  BrowserTestUtils.removeTab(tab);
  SidebarController.hide();
  await SpecialPowers.popPrefEnv();
});
