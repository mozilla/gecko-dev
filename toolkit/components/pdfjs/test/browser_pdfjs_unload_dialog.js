/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const RELATIVE_DIR = "toolkit/components/pdfjs/test/";
const TESTROOT = "https://example.com/browser/" + RELATIVE_DIR;

var MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window.browsingContext);

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/content/tests/browser/common/mockTransfer.js",
  this
);

function createPromiseForTransferComplete(expectedFileName, destFile) {
  return new Promise(resolve => {
    MockFilePicker.showCallback = fp => {
      info("Filepicker shown, checking filename");
      is(fp.defaultString, expectedFileName, "Filename should be correct.");
      let fileName = fp.defaultString;
      destFile.append(fileName);

      MockFilePicker.setFiles([destFile]);
      MockFilePicker.filterIndex = 0; // kSaveAsType_Complete

      MockFilePicker.showCallback = null;
      mockTransferCallback = function (downloadSuccess) {
        ok(downloadSuccess, "File should have been downloaded successfully");
        mockTransferCallback = () => {};
        resolve();
      };
    };
  });
}

let tempDir = createTemporarySaveDirectory();

add_setup(async function () {
  mockTransferRegisterer.register();
  registerCleanupFunction(function () {
    mockTransferRegisterer.unregister();
    MockFilePicker.cleanup();
    tempDir.remove(true);
  });
});

function getDialogObserver(buttonN) {
  const onCommonDialogLoaded = async ({
    Dialog: {
      ui: { button0, button1, button2, infoBody, infoTitle },
    },
  }) => {
    Services.obs.removeObserver(onCommonDialogLoaded, "common-dialog-loaded");
    ok(infoTitle.textContent, "Save PDF before leaving?");
    ok(
      infoBody.textContent,
      "Save this document to avoid losing your changes."
    );
    [button0, button1, button2][buttonN].click();
    if (buttonN !== 1) {
      await waitForPdfJSClose(gBrowser.selectedBrowser);
    }
  };
  return onCommonDialogLoaded;
}

async function setUp(browser) {
  await waitForPdfJSAnnotationLayer(browser, TESTROOT + "file_pdfjs_form.pdf");

  await clickOn(browser, "#pdfjs_internal_id_7R");
  await write(browser, "hello world");
  await clickOn(browser, ".textLayer span");

  await SpecialPowers.spawn(browser, [], async () => {
    const { annotationStorage } =
      content.wrappedJSObject.PDFViewerApplication.pdfDocument;
    ok(
      annotationStorage.has("7R"),
      "Annotation storage should have the annotation"
    );
  });

  const tab = gBrowser.getTabForBrowser(browser);
  tab.closeButton.click();

  return tab;
}

add_task(async function test_dontsave_dialog_when_leaving_unsaved_form() {
  Services.obs.addObserver(getDialogObserver(2), "common-dialog-loaded");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (browser) {
      const tab = await setUp(browser);
      ok(tab.closing, "Tab should be closing");
    }
  );
});

add_task(async function test_save_dialog_when_leaving_unsaved_form() {
  Services.obs.addObserver(getDialogObserver(0), "common-dialog-loaded");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (browser) {
      const destFile = tempDir.clone();
      MockFilePicker.displayDirectory = tempDir;
      const fileSavedPromise = createPromiseForTransferComplete(
        "file_pdfjs_form.pdf",
        destFile
      );
      const tab = await setUp(browser);
      await fileSavedPromise;
      ok(tab.closing, "Tab should be closing");
    }
  );
});

add_task(async function test_cancel_dialog_when_leaving_unsaved_form() {
  Services.obs.addObserver(getDialogObserver(1), "common-dialog-loaded");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (browser) {
      const tab = await setUp(browser);
      ok(!tab.closing, "Tab should not be closing");
      await waitForPdfJSClose(browser);
    }
  );
});
