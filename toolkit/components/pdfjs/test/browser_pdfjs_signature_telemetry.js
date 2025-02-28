/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { PdfJsTelemetry } = ChromeUtils.importESModule(
  "resource://pdf.js/PdfJsTelemetry.sys.mjs"
);
const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

const RELATIVE_DIR = "toolkit/components/pdfjs/test/";
const TESTROOT = "https://example.com/browser/" + RELATIVE_DIR;

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/content/tests/browser/common/mockTransfer.js",
  this
);

const MockFilePicker = SpecialPowers.MockFilePicker;
const file = new FileUtils.File(getTestFilePath("moz.png"));
const signaturePref = "pdfjs.enableSignatureEditor";

add_setup(async function () {
  requestLongerTimeout(2);
  MockFilePicker.init(window.browsingContext);
  MockFilePicker.setFiles([file]);
  MockFilePicker.returnValue = MockFilePicker.returnOK;
  registerCleanupFunction(function () {
    MockFilePicker.cleanup();
  });
});

const sandbox = sinon.createSandbox();
registerCleanupFunction(() => {
  sandbox.restore();
});

const original = PdfJsTelemetry.report.bind(PdfJsTelemetry);
const resolvers = new Map();
sandbox.stub(PdfJsTelemetry, "report").callsFake(aData => {
  const { data } = aData;
  if (!data) {
    return;
  }
  const { action } = data;
  const name = action ? action.split(".").pop() : data.type;
  resolvers.get(name)?.resolve();
  original(aData);
});
const getPromise = name => {
  const resolver = Promise.withResolvers();
  resolvers.set(name, resolver);
  return resolver.promise;
};
let telemetryPromise;

async function enableSignature(browser) {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["pdfjs.annotationEditorMode", 0],
      [signaturePref, true],
    ],
  });

  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();

  await waitForPdfJSAllLayers(browser, TESTROOT + "file_empty_test.pdf", [
    ["annotationEditorLayer", "annotationLayer", "textLayer", "canvasWrapper"],
  ]);

  await SpecialPowers.spawn(browser, [], () => {
    content.addEventListener(
      "storedSignaturesChanged",
      e => {
        e.stopImmediatePropagation();
        e.preventDefault();
        e.stopPropagation();
      },
      { capture: true }
    );
  });
  await enableEditor(browser, "Signature", 0);

  await Services.fog.testFlushAllChildren();
  Assert.equal(
    Glean.pdfjs.editing.signature.testGetValue() || 0,
    0,
    "Should have no signature"
  );
}

registerCleanupFunction(async function () {
  const request = indexedDB.deleteDatabase("pdfjs");
  const { promise, resolve, reject } = Promise.withResolvers();
  request.onsuccess = resolve;
  request.onerror = reject;

  try {
    await promise;
  } catch {
    is(false, "The DB must be deleted");
  }
});

// Test telemetry for signature flow.

add_task(async function test_telemetry_clear_signature() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (browser) {
      await enableSignature(browser);

      telemetryPromise = getPromise("added");
      await clickOn(browser, "#editorSignatureAddSignature");
      await waitForSelector(browser, "#addSignatureDialog");

      await telemetryPromise;
      await Services.fog.testFlushAllChildren();
      let value = Glean.pdfjs.editing.signature.testGetValue();
      Assert.equal(value, 1, "Should have a signature added");

      // Test the type panel.

      await clickOn(browser, "#addSignatureTypeInput");
      await write(browser, "Test");

      telemetryPromise = getPromise("clear");
      await clickOn(browser, "#clearSignatureButton");

      await Services.fog.testFlushAllChildren();
      value = Glean.pdfjsSignature.clear.type.testGetValue();
      Assert.equal(value, 1, "A typed signature should be cleared");

      // Test the draw panel.

      await clickOn(browser, "#addSignatureDrawButton");
      await clickOn(browser, "#addSignatureDraw");
      await waitForSelector(browser, `#addSignatureDraw > path:not([d=""])`);

      telemetryPromise = getPromise("clear");
      await clickOn(browser, "#clearSignatureButton");

      await Services.fog.testFlushAllChildren();
      value = Glean.pdfjsSignature.clear.type.testGetValue();
      Assert.equal(value, 1, "A drawn signature should be cleared");

      // Test the image panel.

      await clickOn(browser, "#addSignatureImageButton");
      await clickOn(browser, "#addSignatureImageBrowse");
      await waitForSelector(browser, `#addSignatureImage > path:not([d=""])`);

      telemetryPromise = getPromise("clear");
      await clickOn(browser, "#clearSignatureButton");

      await Services.fog.testFlushAllChildren();
      value = Glean.pdfjsSignature.clear.type.testGetValue();
      Assert.equal(value, 1, "A drawn signature should be cleared");

      await clickOn(browser, "#addSignatureCancelButton");

      await waitForPdfJSClose(browser);
      await SpecialPowers.popPrefEnv();
    }
  );
});

add_task(async function test_telemetry_create_and_delete() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (browser) {
      await enableSignature(browser);

      // Test adding a typed signature.

      await clickOn(browser, "#editorSignatureAddSignature");
      await waitForSelector(browser, "#addSignatureDialog");

      await clickOn(browser, "#addSignatureTypeInput");
      await write(browser, "Test abc");

      telemetryPromise = getPromise("created");
      await clickOn(browser, "#addSignatureAddButton");
      await telemetryPromise;
      await testTelemetryEventExtra(
        Glean.pdfjsSignature.created,
        [
          {
            type: "type",
            saved: "true",
            saved_count: "1",
            description_changed: "false",
          },
        ],
        false
      );

      Services.fog.testResetFOG();

      telemetryPromise = getPromise("inserted");
      await clickOn(browser, "button.toolbarAddSignatureButton");
      await telemetryPromise;
      await testTelemetryEventExtra(
        Glean.pdfjsSignature.inserted,
        [
          {
            has_been_saved: "true",
            has_description: "true",
          },
        ],
        false
      );

      Services.fog.testResetFOG();

      // Test adding a drawn signature.

      await clickOn(browser, "#editorSignatureAddSignature");
      await waitForSelector(browser, "#addSignatureDialog");

      await clickOn(browser, "#addSignatureDrawButton");
      await clickOn(browser, "#addSignatureDraw");
      await waitForSelector(browser, `#addSignatureDraw > path:not([d=""])`);
      await clickOn(browser, "#addSignatureDescription > input");
      await write(browser, "Changed description");

      telemetryPromise = getPromise("created");
      await clickOn(browser, "#addSignatureAddButton");

      await telemetryPromise;
      await testTelemetryEventExtra(
        Glean.pdfjsSignature.created,
        [
          {
            type: "draw",
            saved: "true",
            saved_count: "2",
            description_changed: "true",
          },
        ],
        false
      );

      Services.fog.testResetFOG();

      // Test adding a signature from an image.

      await clickOn(browser, "#editorSignatureAddSignature");
      await waitForSelector(browser, "#addSignatureDialog");

      await clickOn(browser, "#addSignatureImageButton");
      await clickOn(browser, "#addSignatureImageBrowse");
      await waitForSelector(browser, `#addSignatureImage > path:not([d=""])`);
      await waitForSelector(
        browser,
        `#addSignatureSaveContainer:not([disabled="true"])`
      );
      await clickOn(browser, "#addSignatureSaveCheckbox");

      telemetryPromise = getPromise("created");
      await clickOn(browser, "#addSignatureAddButton");
      await telemetryPromise;
      await testTelemetryEventExtra(
        Glean.pdfjsSignature.created,
        [
          {
            type: "image",
            saved: "false",
            saved_count: "2",
            description_changed: "false",
          },
        ],
        false
      );

      Services.fog.testResetFOG();

      // Test deleting a saved signature.

      telemetryPromise = getPromise("delete_saved");
      await clickOn(
        browser,
        ".toolbarAddSignatureButtonContainer > button.deleteButton"
      );
      await telemetryPromise;
      await testTelemetryEventExtra(
        Glean.pdfjsSignature.deleteSaved,
        [
          {
            saved_count: "1",
          },
        ],
        false
      );

      await waitForPdfJSClose(browser);
      await SpecialPowers.popPrefEnv();
    }
  );
});

add_task(async function test_telemetry_print() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (browser) {
      await enableSignature(browser);

      await clickOn(browser, "#editorSignatureAddSignature");
      await waitForSelector(browser, "#addSignatureDialog");

      await clickOn(browser, "#addSignatureTypeInput");
      await write(browser, "Test abc");
      await clickOn(browser, "#addSignatureAddButton");
      await waitForSelector(browser, ".signatureEditor .editDescription");

      Services.fog.testResetFOG();
      telemetryPromise = getPromise("print");
      await clickOn(browser, "#printButton");

      await waitForPreviewVisible();
      await closePreview();

      await telemetryPromise;
      await testTelemetryEventExtra(
        Glean.pdfjsSignature.added,
        [
          {
            has_alt_text: "1",
            has_no_alt_text: "0",
          },
        ],
        false
      );

      await waitForPdfJSClose(browser);
      await SpecialPowers.popPrefEnv();
    }
  );
});

add_task(async function test_telemetry_edit_dscription() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (browser) {
      await enableSignature(browser);

      await clickOn(browser, "#editorSignatureAddSignature");
      await waitForSelector(browser, "#addSignatureDialog");

      await clickOn(browser, "#addSignatureTypeInput");
      await write(browser, "Test abc");
      await clickOn(browser, "#addSignatureAddButton");

      await clickOn(browser, "button.editDescription");
      await waitForSelector(browser, "#editSignatureDescriptionDialog");

      Services.fog.testResetFOG();
      telemetryPromise = getPromise("edit_description");
      await clickOn(browser, "#editSignatureCancelButton");

      await telemetryPromise;
      let value = Glean.pdfjsSignature.editDescription.unsaved.testGetValue();
      Assert.equal(value, 1, "Should have an unsaved description");

      await clickOn(browser, "button.editDescription");
      await waitForSelector(browser, "#editSignatureDescriptionDialog");
      await clickOn(browser, "#editSignatureDescription");
      await write(browser, "Changed description");

      Services.fog.testResetFOG();
      telemetryPromise = getPromise("edit_description");
      await clickOn(browser, "#editSignatureUpdateButton");

      await telemetryPromise;
      value = Glean.pdfjsSignature.editDescription.saved.testGetValue();
      Assert.equal(value, 1, "Should have a saved description");

      await waitForPdfJSClose(browser);
      await SpecialPowers.popPrefEnv();
    }
  );
});
