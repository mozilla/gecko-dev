/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const RELATIVE_DIR = "toolkit/components/pdfjs/test/";
const TESTROOT = "https://example.com/browser/" + RELATIVE_DIR;
const pdfUrl = TESTROOT + "file_pdfjs_test.pdf";
const toolbarDensityPref = "browser.uidensity";

// Test that changing the toolbar density pref is dispatched in pdf.js.
add_task(async function test() {
  const mimeService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
  const handlerInfo = mimeService.getFromTypeAndExtension(
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

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function test_updated_preferences(browser) {
      await waitForPdfJS(browser, pdfUrl);

      const prefs = {
        enableGuessAltText: {
          type: "Bool",
          initialValue: true,
          newValue: false,
          expectedValue: false,
          listenForUpdate: true,
        },
        pdfBugEnabled: {
          type: "Bool",
          initialValue: false,
          newValue: true,
          // pdfBugEnabled is immutable.
          expectedValue: false,
          listenForUpdate: false,
        },
      };

      for (const pref of Object.keys(prefs)) {
        Services.prefs.clearUserPref(`pdfjs.${pref}`);
      }

      const promises = [];
      for (const [
        pref,
        { type, initialValue, listenForUpdate },
      ] of Object.entries(prefs)) {
        Assert.strictEqual(
          Services.prefs[`get${type}Pref`](`pdfjs.${pref}`),
          initialValue,
          `pdfjs.${pref} should be ${initialValue}`
        );
        if (listenForUpdate) {
          promises.push(
            BrowserTestUtils.waitForContentEvent(
              browser,
              "updatedPreference",
              false,
              null,
              true
            )
          );
        }
      }

      await SpecialPowers.spawn(browser, [prefs], async prfs => {
        const viewer = content.wrappedJSObject.PDFViewerApplication;
        for (const [pref, { newValue }] of Object.entries(prfs)) {
          await viewer.preferences.set(pref, newValue);
        }
      });

      await Promise.all(promises);

      for (const [pref, { type, expectedValue }] of Object.entries(prefs)) {
        Assert.strictEqual(
          Services.prefs[`get${type}Pref`](`pdfjs.${pref}`),
          expectedValue,
          `pdfjs.${pref} should be ${expectedValue}`
        );
      }

      for (const pref of Object.keys(prefs)) {
        Services.prefs.clearUserPref(`pdfjs.${pref}`);
      }

      await waitForPdfJSClose(browser);
    }
  );
});
