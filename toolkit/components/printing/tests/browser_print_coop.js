/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

const TEST_PATH_SITE = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://test1.example.com"
);

// Verify if the page with a COOP header can be used for printing preview.
add_task(async function test_print_coop_basic() {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();
    ok(true, "We did not crash.");
    await helper.closeDialog();
  }, "file_coop_header.html");
});

add_task(async function test_window_print_coop() {
  for (const base of [TEST_PATH, TEST_PATH_SITE]) {
    for (const query of [
      "print",
      "print-same-origin-frame",
      "print-cross-origin-frame",
    ]) {
      const url = `${base}file_coop_header.html?${query}`;
      info(`Testing ${url}`);
      is(
        document.querySelector(".printPreviewBrowser"),
        null,
        "There shouldn't be any print preview browser"
      );
      await BrowserTestUtils.withNewTab(url, async function (browser) {
        await new PrintHelper(browser).waitForDialog();

        isnot(
          document.querySelector(".printPreviewBrowser"),
          null,
          "Should open the print preview correctly"
        );

        ok(true, "Shouldn't crash");
        gBrowser.getTabDialogBox(browser).abortAllDialogs();
      });
    }
  }
});
