/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { DownloadIntegration } = ChromeUtils.importESModule(
  "resource://gre/modules/DownloadIntegration.sys.mjs"
);

const HandlerService = Cc[
  "@mozilla.org/uriloader/handler-service;1"
].getService(Ci.nsIHandlerService);
const MIMEService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

const { saveToDisk, alwaysAsk, handleInternally, useSystemDefault } =
  Ci.nsIHandlerInfo;

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.always_ask_before_handling_new_types", false],
      ["browser.download.useDownloadDir", false],
    ],
  });

  registerCleanupFunction(async () => {
    let publicList = await Downloads.getList(Downloads.PUBLIC);
    await publicList.removeFinished();

    if (DownloadsPanel.isVisible) {
      let hiddenPromise = BrowserTestUtils.waitForEvent(
        DownloadsPanel.panel,
        "popuphidden"
      );
      DownloadsPanel.hidePanel();
      await hiddenPromise;
    }

    let mimeInfo = MIMEService.getFromTypeAndExtension(
      "application/pdf",
      "pdf"
    );
    let existed = HandlerService.exists(mimeInfo);
    if (existed) {
      HandlerService.store(mimeInfo);
    } else {
      HandlerService.remove(mimeInfo);
    }
  });
});

async function onFilePickerShown(test) {
  const { MockFilePicker } = SpecialPowers;

  MockFilePicker.init(window.browsingContext);
  MockFilePicker.returnValue = MockFilePicker.returnOK;

  const promise = new Promise(resolve => {
    MockFilePicker.showCallback = fp => {
      ok(true, "filepicker should be visible");
      Assert.strictEqual(
        fp.defaultExtension,
        "pdf",
        "Default extension in filepicker should be pdf"
      );
      Assert.strictEqual(
        fp.defaultString,
        "file_pdf.pdf",
        "Default string name in filepicker should have the correct pdf file name"
      );
      setTimeout(resolve, 0);
      return Ci.nsIFilePicker.returnCancel;
    };
  });

  test();

  await promise;
  MockFilePicker.reset();
}

function waitForAcceptButtonToGetEnabled(doc) {
  let dialog = doc.querySelector("#unknownContentType");
  let button = dialog.getButton("accept");
  return TestUtils.waitForCondition(
    () => !button.disabled,
    "Wait for Accept button to get enabled"
  );
}

async function onExternalApplication(test) {
  let loaded = BrowserTestUtils.domWindowOpenedAndLoaded();
  test();
  const win = await loaded;
  is(
    win.location.href,
    "chrome://mozapps/content/downloads/unknownContentType.xhtml",
    "Should have seen the unknown content dialogWindow."
  );

  const doc = win.document;
  await waitForAcceptButtonToGetEnabled(doc);
  const dialog = doc.querySelector("#unknownContentType");
  dialog.cancelDialog();
}

async function onDownload(test) {
  const downloadsList = await Downloads.getList(Downloads.PUBLIC);
  const savePromise = promiseDownloadFinished(downloadsList, true);
  test();
  await savePromise;
  ok(true, "Download finished");
}

async function onBlockedBySandbox(test) {
  const expected = `Download of “${TEST_PATH}file_pdf.pdf” was blocked because the triggering iframe has the sandbox flag set.`;
  return new Promise(resolve => {
    Services.console.registerListener(function onMessage(msg) {
      let { message, logLevel } = msg;
      if (logLevel != Ci.nsIConsoleMessage.warn) {
        return;
      }
      if (!message.includes(expected)) {
        return;
      }
      Services.console.unregisterListener(onMessage);
      resolve();
    });

    test();
  });
}

function autopass(test) {
  ok(true);
  test();
}

const tests = [
  {
    preferredAction: alwaysAsk,
    runTest: onExternalApplication,
    header: "preferredAction = alwaysAsk",
  },
  {
    preferredAction: saveToDisk,
    runTest: onFilePickerShown,
    header: "preferredAction = saveToDisk",
  },
  {
    preferredAction: handleInternally,
    runTest: onBlockedBySandbox,
    header: "preferredAction = handleInternally",
  },
  {
    preferredAction: useSystemDefault,
    runTest: onDownload,
    header: "preferredAction = useSystemDefault",
  },
];

/**
 * Tests that selecting the context menu item `Save Link As…` on a PDF link
 * opens the file picker when always_ask_before_handling_new_types is disabled,
 * regardless of preferredAction.
 */
add_task(async function test_pdf_save_as_link() {
  let mimeInfo;

  for (let { preferredAction, runTest, header } of tests) {
    mimeInfo = MIMEService.getFromTypeAndExtension("application/pdf", "pdf");
    mimeInfo.alwaysAskBeforeHandling = preferredAction === alwaysAsk;
    mimeInfo.preferredAction = preferredAction;
    HandlerService.store(mimeInfo);

    info(`Running test: ${header}`);

    await runTest(() => {
      gBrowser.selectedTab = BrowserTestUtils.addTab(
        gBrowser,
        `data:text/html,<!doctype html><iframe sandbox="allow-downloads" src=${TEST_PATH}file_pdf.pdf></iframe>`
      );
    });

    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
});
