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
    async function test_toolbar_density_dispatch(browser) {
      await waitForPdfJS(browser, pdfUrl);

      let promise = BrowserTestUtils.waitForContentEvent(
        browser,
        "toolbardensity",
        true,
        ({ detail: { value } }) => value === 2,
        true
      );
      await SpecialPowers.pushPrefEnv({
        set: [[toolbarDensityPref, 2]],
      });
      await promise;

      promise = BrowserTestUtils.waitForContentEvent(
        browser,
        "toolbardensity",
        false,
        ({ detail: { value } }) => value === 0,
        true
      );
      await SpecialPowers.popPrefEnv();
      await promise;

      await waitForPdfJSClose(browser);
    }
  );
});
