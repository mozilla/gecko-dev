/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/ml/tests/browser/head.js",
  this
);

const RELATIVE_DIR = "toolkit/components/pdfjs/test/";
const TESTROOT = "https://example.com/browser/" + RELATIVE_DIR;
const pdfUrl = TESTROOT + "file_pdfjs_test.pdf";
const altTextPref = "pdfjs.enableAltText";
const guessAltTextPref = "pdfjs.enableGuessAltText";
const browserMLPref = "browser.ml.enable";
const annotationEditorModePref = "pdfjs.annotationEditorMode";

async function setupRemoteClient() {
  const { removeMocks, remoteClients } = await createAndMockMLRemoteSettings({
    autoDownloadFromRemoteSettings: false,
  });

  return {
    remoteClients,
    async cleanup() {
      await removeMocks();
      await waitForCondition(
        () => EngineProcess.areAllEnginesTerminated(),
        "Waiting for all of the engines to be terminated.",
        100,
        200
      );
    },
  };
}

add_task(async function test_loaded_model_in_two_tabs() {
  let mimeService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
  let handlerInfo = mimeService.getFromTypeAndExtension(
    "application/pdf",
    "pdf"
  );

  // Make sure pdf.js is the default handler.
  is(
    handlerInfo.alwaysAskBeforeHandling,
    false,
    "pdf handler defaults to always-ask is false"
  );
  is(
    handlerInfo.preferredAction,
    Ci.nsIHandlerInfo.handleInternally,
    "pdf handler defaults to internal"
  );

  info("Pref action: " + handlerInfo.preferredAction);

  await SpecialPowers.pushPrefEnv({
    set: [
      [altTextPref, true],
      [browserMLPref, true],
      [guessAltTextPref, true],
      [annotationEditorModePref, 0],
    ],
  });
  const setRC = await setupRemoteClient();
  const { EngineProcess } = ChromeUtils.importESModule(
    "chrome://global/content/ml/EngineProcess.sys.mjs"
  );

  const browsers = [];
  for (let i = 0; i < 2; i++) {
    const tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      "about:blank"
    );
    const browser = tab.linkedBrowser;
    browsers.push(browser);

    await waitForPdfJS(browser, pdfUrl);
    await enableEditor(browser, "Stamp", 1);

    const isEnabled = await SpecialPowers.spawn(browser, [], () => {
      const viewer = content.wrappedJSObject.PDFViewerApplication;
      return viewer.mlManager.isEnabledFor("altText");
    });
    ok(isEnabled, `AltText is enabled in tab number ${i + 1}`);
  }

  for (const browser of browsers) {
    await waitForPdfJSClose(browser, /* closeTab = */ true);
  }

  await setRC.remoteClients["ml-onnx-runtime"].resolvePendingDownloads(1);
  await EngineProcess.destroyMLEngine();
  await setRC.cleanup();

  await SpecialPowers.popPrefEnv();
});
