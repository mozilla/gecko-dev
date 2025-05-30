/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const RELATIVE_DIR = "toolkit/components/pdfjs/test/";
const TESTROOT = "https://example.com/browser/" + RELATIVE_DIR;

async function test_pdfjs_print(enabled, browser) {
  makePDFJSHandler();

  await SpecialPowers.pushPrefEnv({
    set: [["print.enabled", enabled]],
  });

  await waitForPdfJS(browser, TESTROOT + "file_pdfjs_test.pdf");
  await SpecialPowers.spawn(browser, [enabled], async enabled => {
    const printButton = content.document.querySelector("#printButton");
    const displayed = content.getComputedStyle(printButton).display !== "none";
    Assert.equal(
      displayed,
      enabled,
      `Print button is ${enabled ? "enabled" : "disabled"}`
    );
  });

  await waitForPdfJSClose(browser);
  await SpecialPowers.popPrefEnv();
}

add_task(async function test_print_enabled() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    test_pdfjs_print.bind(null, true)
  );
});

add_task(async function test_print_disabled() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    test_pdfjs_print.bind(null, false)
  );
});
