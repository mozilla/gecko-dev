/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let mockCA = makeMockContentAnalysis();

add_setup(async function test_setup() {
  mockCA = await mockContentAnalysisService(mockCA);
});

const testPage =
  "<body style='margin: 0'><input id='input' type='text'></body>";

const CLIPBOARD_TEXT_STRING = "Just some text";

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

async function getElementValue(browser, elementId) {
  return await SpecialPowers.spawn(browser, [elementId], async elementId => {
    return content.document.getElementById(elementId).value;
  });
}

async function testBusyDialog(cancel) {
  mockCA.setupForTest(true, /* waitForEvent */ true, /* showDialogs */ true);
  let tab = BrowserTestUtils.addTab(gBrowser);
  let browser = gBrowser.getBrowserForTab(tab);
  gBrowser.selectedTab = tab;
  await promiseTabLoadEvent(tab, "data:text/html," + escape(testPage));
  await SimpleTest.promiseFocus(browser);

  setClipboardData(CLIPBOARD_TEXT_STRING);

  let scanStartedPromise = new Promise(res => {
    mockCA.eventTarget.addEventListener(
      "inAnalyzeContentRequest",
      () => {
        res();
      },
      { once: true }
    );
  });

  // Paste into content
  await SpecialPowers.spawn(browser, [], () => {
    content.document.getElementById("input").value = "";
    content.document.getElementById("input").focus();
  });

  // Wait for busy dialog and click cancel if specified
  let dialogPromise = BrowserTestUtils.promiseAlertDialogOpen();
  let doPastePromise = BrowserTestUtils.synthesizeKey(
    "v",
    { accelKey: true },
    browser
  );
  await scanStartedPromise;

  let win = await dialogPromise;
  let dialog = win.document.querySelector("dialog");

  if (cancel) {
    dialog.getButton("cancel").click();
    await TestUtils.waitForCondition(
      () => mockCA.agentCancelCalls === 1,
      "checking number of cancel calls after dialog closed"
    );
  }
  // Make the request complete.
  mockCA.eventTarget.dispatchEvent(
    new CustomEvent("returnContentAnalysisResponse")
  );

  await doPastePromise;
  ok(!dialog.open, "dialog should not be open");

  is(
    mockCA.agentCancelCalls,
    cancel ? 1 : 0,
    "checking number of cancel calls after request finishes"
  );
  is(
    await getElementValue(browser, "input"),
    cancel ? "" : CLIPBOARD_TEXT_STRING,
    "checking text field contents"
  );

  BrowserTestUtils.removeTab(tab);
}

async function testClosingTab() {
  mockCA.setupForTest(true, /* waitForEvent */ true, /* showDialogs */ true);
  let tab = BrowserTestUtils.addTab(gBrowser);
  let browser = gBrowser.getBrowserForTab(tab);
  gBrowser.selectedTab = tab;
  await promiseTabLoadEvent(tab, "data:text/html," + escape(testPage));
  await SimpleTest.promiseFocus(browser);

  setClipboardData(CLIPBOARD_TEXT_STRING);

  let scanStartedPromise = new Promise(res => {
    mockCA.eventTarget.addEventListener(
      "inAnalyzeContentRequest",
      () => {
        res();
      },
      { once: true }
    );
  });

  // Paste into content
  await SpecialPowers.spawn(browser, [], () => {
    content.document.getElementById("input").value = "";
    content.document.getElementById("input").focus();
  });

  let dialogPromise = BrowserTestUtils.promiseAlertDialogOpen();
  let doPastePromise = BrowserTestUtils.synthesizeKey(
    "v",
    { accelKey: true },
    browser
  );
  await scanStartedPromise;

  let win = await dialogPromise;
  let dialog = win.document.querySelector("dialog");

  BrowserTestUtils.removeTab(tab);
  await TestUtils.waitForCondition(
    () => mockCA.agentCancelCalls === 1,
    "checking number of cancel calls after tab closed"
  );

  // Make the request complete.
  mockCA.eventTarget.dispatchEvent(
    new CustomEvent("returnContentAnalysisResponse")
  );

  await doPastePromise;
  ok(!dialog.open, "dialog should not be open");
}

// Ensure that when a DLP request is made, the busy dialog is shown
// and when cancel is clicked, the request is cancelled and the
// paste is not done.
add_task(async function testBusyConfirmationDialogWithCancel() {
  await testBusyDialog(true);
});

// Ensure that when a DLP request is made, the busy dialog is shown
// and when the analysis is done the busy dialog goes away and the
// paste is done.
add_task(async function testBusyConfirmationDialogWithNoCancel() {
  await testBusyDialog(false);
});

// Ensure that when a DLP request is made and then the tab is closed,
// the busy dialog is closed and the request is cancelled.
add_task(async function testClosingTabCancelsRequest() {
  await testClosingTab();
});
