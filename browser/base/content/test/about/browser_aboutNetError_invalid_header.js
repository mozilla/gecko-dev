/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const INVALID_HEADER =
  "https://example.com/browser/browser/base/content/test/about/invalid_header.sjs";

// From appstrings.properties
const EXPECTED_SHORT_DESC =
  INVALID_HEADER +
  " sent back a header with empty characters not allowed by web security standards.";

add_task(async function test_invalidHeaderValue() {
  let browser;
  let pageLoaded;

  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    () => {
      gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, INVALID_HEADER);
      browser = gBrowser.selectedBrowser;
      pageLoaded = BrowserTestUtils.waitForErrorPage(browser);
    },
    false
  );

  await pageLoaded;

  await SpecialPowers.spawn(
    browser,
    [EXPECTED_SHORT_DESC],
    function (expectedShortDesc) {
      const doc = content.document;
      ok(
        doc.documentURI.startsWith("about:neterror"),
        "Should be showing error page"
      );

      const titleEl = doc.querySelector(".title-text");
      const actualDataL10nID = titleEl.getAttribute("data-l10n-id");
      is(
        actualDataL10nID,
        "problem-with-this-site-title",
        "Correct error link title is set"
      );
      const shortDesc = doc.getElementById("errorShortDesc");
      ok(
        shortDesc.textContent.startsWith(expectedShortDesc),
        "Correct error page title is set"
      );
    }
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
