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

const COOP_HELPER_FILE =
  getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "") +
  "file_print_coop_helper.html";

async function test_print_flags_with_headers(headers, expectIsolated) {
  let params = new URLSearchParams({
    file: COOP_HELPER_FILE,
  });
  for (let header of headers) {
    params.append("headers", header);
  }
  for (const origin of ["https://example.com", "https://test1.example.com"]) {
    for (const hash of [
      "print",
      "print-same-origin-frame",
      "print-same-origin-frame-srcdoc",
      "print-cross-origin-frame",
    ]) {
      if (expectIsolated && hash == "print-cross-origin-frame") {
        // The iframe wouldn't load.
        continue;
      }
      let url = `${origin}/document-builder.sjs?${params}#${hash}`;
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
        let isolated = await SpecialPowers.spawn(browser, [], () => {
          return content.crossOriginIsolated;
        });
        is(isolated, expectIsolated, "Expected isolation");
      });
    }
  }
}

add_task(async function test_window_print_coop() {
  return test_print_flags_with_headers(
    ["Cross-Origin-Opener-Policy: same-origin"],
    /* expectIsolated = */ false
  );
});

add_task(async function test_window_print_coep() {
  return test_print_flags_with_headers(
    [
      "Cross-Origin-Opener-Policy: same-origin",
      "Cross-Origin-Embedder-Policy: require-corp",
    ],
    /* expectIsolated = */ true
  );
});
