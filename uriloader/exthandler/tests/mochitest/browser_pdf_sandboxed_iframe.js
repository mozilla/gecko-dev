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

async function onBlockedBySandbox(test, file) {
  const expected = `Download of “${TEST_PATH}${file}” was blocked because the triggering iframe has the sandbox flag set.`;
  return new Promise(resolve => {
    Services.console.registerListener(async function onMessage(msg) {
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
    file: "file_pdf.pdf",
    sandbox: "allow-downloads",
  },
  {
    preferredAction: saveToDisk,
    runTest: onFilePickerShown,
    header: "preferredAction = saveToDisk",
    file: "file_pdf.pdf",
    sandbox: "allow-downloads",
  },
  {
    // The preferredAction handleInternally is only noticed when we're getting
    // an externally handled PDF that we forcefully handle internally.
    preferredAction: handleInternally,
    runTest: onBlockedBySandbox,
    header: "preferredAction = handleInternally",
    prefs: ["browser.download.open_pdf_attachments_inline", true],
    file: "file_pdf_content_disposition.pdf",
    sandbox: "allow-downloads",
  },
  {
    preferredAction: useSystemDefault,
    runTest: onDownload,
    header: "preferredAction = useSystemDefault",
    file: "file_pdf.pdf",
    sandbox: "allow-downloads",
  },
  {
    preferredAction: alwaysAsk,
    runTest: onBlockedBySandbox,
    header: "preferredAction = alwaysAsk",
    file: "file_pdf.pdf",
  },
  {
    preferredAction: saveToDisk,
    runTest: onBlockedBySandbox,
    header: "preferredAction = saveToDisk",
    file: "file_pdf.pdf",
  },
  {
    // The preferredAction handleInternally is only noticed when we're getting
    // an externally handled PDF that we forcefully handle internally.
    preferredAction: handleInternally,
    runTest: onBlockedBySandbox,
    header: "preferredAction = handleInternally",
    prefs: ["browser.download.open_pdf_attachments_inline", true],
    file: "file_pdf_content_disposition.pdf",
  },
  {
    preferredAction: useSystemDefault,
    runTest: onBlockedBySandbox,
    header: "preferredAction = useSystemDefault",
    file: "file_pdf.pdf",
  },
];

/**
 * Tests that selecting the context menu item `Save Link As…` on a PDF link
 * opens the file picker when always_ask_before_handling_new_types is disabled,
 * regardless of preferredAction if the iframe has sandbox="allow-downloads".
 */
add_task(async function test_pdf_save_as_link() {
  let mimeInfo;

  for (let {
    preferredAction,
    runTest,
    header,
    prefs,
    file,
    sandbox,
  } of tests) {
    mimeInfo = MIMEService.getFromTypeAndExtension("application/pdf", "pdf");
    mimeInfo.alwaysAskBeforeHandling = preferredAction === alwaysAsk;
    mimeInfo.preferredAction = preferredAction;
    HandlerService.store(mimeInfo);

    info(`Running test: ${header}, ${sandbox ? sandbox : "no sandbox"}`);

    if (prefs) {
      await SpecialPowers.pushPrefEnv({
        set: [prefs],
      });
    }

    await runTest(() => {
      gBrowser.selectedTab = BrowserTestUtils.addTab(
        gBrowser,
        `data:text/html,<!doctype html><iframe sandbox="${sandbox ?? ""}" src=${TEST_PATH}${file}></iframe>`
      );
    }, file);

    if (prefs) {
      await SpecialPowers.popPrefEnv();
    }

    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
});
