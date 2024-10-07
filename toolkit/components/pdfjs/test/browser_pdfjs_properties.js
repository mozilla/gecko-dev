/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const RELATIVE_DIR = "toolkit/components/pdfjs/test/";
const TESTROOT = "https://example.com/browser/" + RELATIVE_DIR;

/**
 * Test that the pdf has the correct color thanks to the SVG filters.
 */
add_task(async function test() {
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

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (browser) {
      // check that PDF is opened with internal viewer
      await waitForPdfJSCanvas(browser, `${TESTROOT}file_pdfjs_test.pdf`);

      await clickOn(browser, "#secondaryToolbarToggleButton");
      await clickOn(browser, "#documentProperties");

      await waitForSelector(browser, "#documentPropertiesDialog");

      const selectorExpected = new Map([
        ["#fileNameField", "file_pdfjs_test.pdf"],
        ["#fileSizeField", "147 KB (150,611 bytes)"],
        ["#titleField", "Untitled"],
        ["#authorField", "-"],
        ["#subjectField", "-"],
        ["#keywordsField", "-"],
        ["#creationDateField", s => s.startsWith("1/17/13, ")],
        ["#modificationDateField", s => s.startsWith("1/17/13, ")],
        ["#creatorField", "PDF24 Creator"],
        ["#producerField", "GPL Ghostscript 9.06"],
        ["#versionField", "1.6"],
        ["#pageCountField", "5"],
        ["#pageSizeField", "8.5 × 11 in (Letter, portrait)"],
        ["#linearizedField", "No"],
      ]);
      for (const [selector, expected] of selectorExpected) {
        await waitForSelector(browser, selector);
        const text = await SpecialPowers.spawn(
          browser,
          [selector],
          async sel => content.document.querySelector(sel).textContent
        );
        if (typeof expected === "function") {
          ok(expected(text), `${selector} must be correct`);
          continue;
        }
        is(text, expected, `${selector} must be correct`);
      }

      await clickOn(browser, "#documentPropertiesClose");

      await waitForPdfJSClose(browser);
    }
  );
});
