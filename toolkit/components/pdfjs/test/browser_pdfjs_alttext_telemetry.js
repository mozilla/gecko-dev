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
const altTextPref = "pdfjs.enableAltText";
const guessAltTextPref = "pdfjs.enableGuessAltText";
const newFlowPref = "pdfjs.enableUpdatedAddImage";
const browserMLPref = "browser.ml.enable";

add_setup(async function () {
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
  // console.log("Telemetry", aData);
  const name = aData.data?.action?.split(".").pop();
  resolvers.get(name)?.resolve();
  original(aData);
});
const getPromise = name => {
  const resolver = Promise.withResolvers();
  resolvers.set(name, resolver);
  return resolver.promise;
};
let telemetryPromise;

// Test telemetry for new alt-text flow.

add_task(async function test_telemetry_new_alt_text_settings() {
  makePDFJSHandler();

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (browser) {
      await SpecialPowers.pushPrefEnv({
        set: [
          ["pdfjs.annotationEditorMode", 0],
          [altTextPref, true],
          [guessAltTextPref, true],
          [newFlowPref, true],
          [browserMLPref, true],
        ],
      });

      await Services.fog.testFlushAllChildren();
      Services.fog.testResetFOG();

      telemetryPromise = getPromise("alt_text_edit");

      // check that PDF is opened with internal viewer
      await waitForPdfJSAllLayers(browser, TESTROOT + "file_empty_test.pdf", [
        [
          "annotationEditorLayer",
          "annotationLayer",
          "textLayer",
          "canvasWrapper",
        ],
      ]);

      await telemetryPromise;
      await Services.fog.testFlushAllChildren();
      let value = Glean.pdfjsImage.altTextEdit.ai_generation.testGetValue();
      Assert.ok(value, "Should have ai_generation enabled");

      value = Glean.pdfjsImage.altTextEdit.ask_to_edit.testGetValue();
      Assert.ok(value, "Should have ask_to_edit enabled");

      // We test the telemetry for the settings dialog.
      await clickOn(browser, "#secondaryToolbarToggle");

      telemetryPromise = getPromise("settings_displayed");
      await clickOn(browser, "#imageAltTextSettings");
      await telemetryPromise;
      await testTelemetryEventExtra(Glean.pdfjsImageAltText.settingsDisplayed, [
        {},
      ]);

      await waitForSelector(browser, "#altTextSettingsDialog");

      for (const [id, record, name] of [
        [
          "createModelButton",
          Glean.pdfjsImageAltText.settingsAiGenerationCheck,
          "settings_ai_generation_check",
        ],
        [
          "showAltTextDialogButton",
          Glean.pdfjsImageAltText.settingsEditAltTextCheck,
          "settings_edit_alt_text_check",
        ],
      ]) {
        telemetryPromise = getPromise(name);
        await clickOn(browser, `#${id}`);
        await telemetryPromise;

        await testTelemetryEventExtra(record, [{ status: "false" }]);

        telemetryPromise = getPromise(name);
        await clickOn(browser, `#${id}`);
        await telemetryPromise;
        await Services.fog.testFlushAllChildren();
        await testTelemetryEventExtra(record, [{ status: "true" }]);
      }

      await SpecialPowers.spawn(browser, [], async () => {
        const a = content.document.querySelector("#altTextSettingsLearnMore");
        a.href = "#";
        a.target = "";
      });
      telemetryPromise = getPromise("info");
      await clickOn(browser, `#altTextSettingsLearnMore`);
      await telemetryPromise;
      await testTelemetryEventExtra(Glean.pdfjsImageAltText.info, [
        { topic: "ai_generation" },
      ]);

      telemetryPromise = getPromise("model_deleted");
      await clickOn(browser, "#deleteModelButton");
      await telemetryPromise;
      await testTelemetryEventExtra(Glean.pdfjsImageAltText.modelDeleted, [{}]);

      telemetryPromise = getPromise("model_download_complete");
      await clickOn(browser, "#downloadModelButton");
      await telemetryPromise;
      await testTelemetryEventExtra(
        Glean.pdfjsImageAltText.modelDownloadStart,
        [{}],
        false
      );
      await testTelemetryEventExtra(
        Glean.pdfjsImageAltText.modelDownloadComplete,
        [{}]
      );

      await clickOn(browser, "#altTextSettingsCloseButton");

      await waitForPdfJSClose(browser);
      await SpecialPowers.popPrefEnv();
    }
  );
});

add_task(async function test_telemetry_new_alt_text_dialog() {
  makePDFJSHandler();

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (browser) {
      await SpecialPowers.pushPrefEnv({
        set: [
          ["pdfjs.annotationEditorMode", 0],
          [altTextPref, true],
          [guessAltTextPref, true],
          [newFlowPref, true],
          [browserMLPref, true],
        ],
      });

      await Services.fog.testFlushAllChildren();
      Services.fog.testResetFOG();

      // check that PDF is opened with internal viewer
      await waitForPdfJSAllLayers(browser, TESTROOT + "file_empty_test.pdf", [
        [
          "annotationEditorLayer",
          "annotationLayer",
          "textLayer",
          "canvasWrapper",
        ],
      ]);

      telemetryPromise = getPromise("icon_click");
      await enableEditor(browser, "Stamp", 0);
      await telemetryPromise;
      await testTelemetryEventExtra(Glean.pdfjsImage.iconClick, [{}]);

      // We test the telemetry for the alt-text dialog.

      telemetryPromise = getPromise("add_image_click");
      const imageSelectedPromise = getPromise("image_selected");
      const modelResultPromise = getPromise("model_result");
      await clickOn(browser, `#editorStampAddImage`);
      await telemetryPromise;
      await testTelemetryEventExtra(
        Glean.pdfjsImage.addImageClick,
        [{}],
        false
      );

      await imageSelectedPromise;
      await testTelemetryEventExtra(
        Glean.pdfjsImage.imageSelected,
        [{ alt_text_modal: "true" }],
        false
      );

      await modelResultPromise;
      await Services.fog.testFlushAllChildren();
      let values = Glean.pdfjsImageAltText.modelResult.testGetValue();
      Assert.equal(values.length, 1, "Should have 1 model result");
      const extra = values[0].extra;
      Assert.ok(extra.time > 0, "time must be a positive number");
      Assert.ok(!!extra.length, "length must be a positive number");

      await waitForSelector(browser, "#newAltTextDialog");

      await SpecialPowers.spawn(browser, [], async () => {
        const a = content.document.querySelector("#newAltTextLearnMore");
        a.href = "#";
        a.target = "";
      });
      telemetryPromise = getPromise("info");
      await clickOn(browser, `#newAltTextLearnMore`);
      await telemetryPromise;
      await testTelemetryEventExtra(Glean.pdfjsImageAltText.info, [
        { topic: "alt_text" },
      ]);

      await SpecialPowers.spawn(browser, [], async function () {
        const { ContentTaskUtils } = ChromeUtils.importESModule(
          "resource://testing-common/ContentTaskUtils.sys.mjs"
        );
        const { document } = content;
        await ContentTaskUtils.waitForCondition(
          () =>
            document.querySelector("#newAltTextDescriptionTextarea").value !==
            "",
          "Textarea mustn't be empty"
        );
      });

      telemetryPromise = getPromise("image_added");
      const savePromise = getPromise("save");
      const statusPromise = getPromise("image_status_label_displayed");
      await clickOn(browser, `#newAltTextSave`);
      await telemetryPromise;
      await testTelemetryEventExtra(
        Glean.pdfjsImage.imageAdded,
        [
          {
            alt_text_modal: "true",
            alt_text_type: "present",
          },
        ],
        false
      );
      await savePromise;
      await testTelemetryEventExtra(
        Glean.pdfjsImageAltText.save,
        [
          {
            alt_text_type: "present",
            flow: "image_add",
          },
        ],
        false
      );
      await statusPromise;
      await testTelemetryEventExtra(
        Glean.pdfjsImageAltText.imageStatusLabelDisplayed,
        [
          {
            label: "added",
          },
        ]
      );

      telemetryPromise = getPromise("image_status_label_clicked");
      await clickOn(browser, ".altText.new");
      await telemetryPromise;
      await testTelemetryEventExtra(
        Glean.pdfjsImageAltText.imageStatusLabelClicked,
        [
          {
            label: "added",
          },
        ]
      );

      await SpecialPowers.spawn(browser, [], async () => {
        const textarea = content.document.querySelector(
          "#newAltTextDescriptionTextarea"
        );
        textarea.value = "Hello Pdf.js World";
      });

      telemetryPromise = getPromise("image_status_label_displayed");
      const userEditPromise = getPromise("user_edit");
      await clickOn(browser, "#newAltTextSave");
      await telemetryPromise;
      await testTelemetryEventExtra(
        Glean.pdfjsImageAltText.imageStatusLabelDisplayed,
        [
          {
            label: "added",
          },
        ],
        false
      );
      await userEditPromise;
      await testTelemetryEventExtra(Glean.pdfjsImageAltText.userEdit, [
        { total_words: 2, words_removed: 2, words_added: 4 },
      ]);

      // Edit again in order to delete the alt text.
      await clickOn(browser, ".altText.new");
      await SpecialPowers.spawn(browser, [], async () => {
        const textarea = content.document.querySelector(
          "#newAltTextDescriptionTextarea"
        );
        textarea.value = "";
      });

      telemetryPromise = getPromise("image_status_label_displayed");
      await clickOn(browser, "#newAltTextSave");
      await telemetryPromise;
      await testTelemetryEventExtra(
        Glean.pdfjsImageAltText.imageStatusLabelDisplayed,
        [
          {
            label: "missing",
          },
        ]
      );

      // Remove the image.
      await clickOn(browser, ".delete");

      await clickOn(browser, `#editorStampAddImage`);

      await SpecialPowers.spawn(browser, [], async () => {
        const { ContentTaskUtils } = ChromeUtils.importESModule(
          "resource://testing-common/ContentTaskUtils.sys.mjs"
        );
        const { document } = content;
        await ContentTaskUtils.waitForCondition(
          () =>
            !!document.querySelector("#newAltTextDescriptionTextarea")?.value,
          "text area must be displayed"
        );
      });
      Services.fog.testResetFOG();

      telemetryPromise = getPromise("image_status_label_displayed");
      const dismissPromise = getPromise("dismiss");
      await clickOn(browser, "#newAltTextNotNow");
      await telemetryPromise;
      await testTelemetryEventExtra(
        Glean.pdfjsImageAltText.imageStatusLabelDisplayed,
        [
          {
            label: "review",
          },
        ],
        false
      );
      await dismissPromise;
      await testTelemetryEventExtra(Glean.pdfjsImageAltText.dismiss, [
        {
          alt_text_type: "present",
          flow: "image_add",
        },
      ]);

      await waitForPdfJSClose(browser);
      await SpecialPowers.popPrefEnv();
    }
  );
});

add_task(async function test_telemetry_new_alt_text_count() {
  makePDFJSHandler();

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (browser) {
      await SpecialPowers.pushPrefEnv({
        set: [
          ["pdfjs.annotationEditorMode", 0],
          [altTextPref, true],
          [guessAltTextPref, true],
          [newFlowPref, true],
          [browserMLPref, true],
        ],
      });

      await Services.fog.testFlushAllChildren();
      Services.fog.testResetFOG();

      // check that PDF is opened with internal viewer
      await waitForPdfJSAllLayers(browser, TESTROOT + "file_empty_test.pdf", [
        [
          "annotationEditorLayer",
          "annotationLayer",
          "textLayer",
          "canvasWrapper",
        ],
      ]);

      await enableEditor(browser, "Stamp", 0);
      await clickOn(browser, `#editorStampAddImage`);

      await SpecialPowers.spawn(browser, [], async () => {
        const { ContentTaskUtils } = ChromeUtils.importESModule(
          "resource://testing-common/ContentTaskUtils.sys.mjs"
        );
        const { document } = content;
        await ContentTaskUtils.waitForCondition(
          () =>
            !!document.querySelector("#newAltTextDescriptionTextarea")?.value,
          "text area must be displayed"
        );
      });
      Services.fog.testResetFOG();

      telemetryPromise = getPromise("alt_text_edit");
      const aiGenCheckPromise = getPromise("ai_generation_check");
      await clickOn(browser, "#newAltTextCreateAutomaticallyButton");
      await telemetryPromise;

      await Services.fog.testFlushAllChildren();
      let value = Glean.pdfjsImage.altTextEdit.ai_generation.testGetValue();
      Assert.ok(!value, "Should have ai_generation disabled");

      await aiGenCheckPromise;
      await testTelemetryEventExtra(Glean.pdfjsImageAltText.aiGenerationCheck, [
        { status: "false" },
      ]);

      await clickOn(browser, "#newAltTextNotNow");

      // Delete the editor and create a new one but without AI.
      await clickOn(browser, ".delete");
      Services.fog.testResetFOG();

      for (const string of ["Hello", "", "World", "", ""]) {
        info("Adding image with alt text: " + (string || "(empty)"));
        await clickOn(browser, `#editorStampAddImage`);
        await waitForSelector(browser, "#newAltTextDescriptionTextarea");
        await SpecialPowers.spawn(browser, [string], async text => {
          content.document.querySelector(
            "#newAltTextDescriptionTextarea"
          ).value = text;
        });
        await clickOn(browser, `#newAltTextSave`);
      }

      await clickOn(browser, "#printButton");

      await waitForPreviewVisible();
      await hitKey(browser, "VK_ESCAPE");

      await Services.fog.testFlushAllChildren();
      value = Glean.pdfjsImage.added.with_alt_text.testGetValue();
      Assert.equal(value, 2, "Should have 2 images with alt text");

      value = Glean.pdfjsImage.added.with_no_alt_text.testGetValue();
      Assert.equal(value, 3, "Should have 3 images without alt text");

      await waitForPdfJSClose(browser);
      await SpecialPowers.popPrefEnv();
    }
  );
});
