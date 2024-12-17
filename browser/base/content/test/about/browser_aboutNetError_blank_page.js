/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BLANK_PAGE =
  "https://example.com/browser/browser/base/content/test/about/blank_page.sjs";

async function test_blankPage(
  page,
  expectedL10nID,
  responseStatus,
  responseStatusText
) {
  let browser;
  let pageLoaded;
  const uri = `${page}?status=${encodeURIComponent(
    responseStatus
  )}&message=${encodeURIComponent(responseStatusText)}`;

  // Simulating loading the page
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    () => {
      gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, uri);
      browser = gBrowser.selectedBrowser;
      pageLoaded = BrowserTestUtils.waitForErrorPage(browser);
    },
    false
  );

  info("Loading and waiting for the net error");
  await pageLoaded;

  await SpecialPowers.spawn(
    browser,
    [expectedL10nID, responseStatus, responseStatusText],
    function (l10nID, expectedStatus, expectedText) {
      const doc = content.document;
      ok(
        doc.documentURI.startsWith("about:neterror"),
        "Should be showing error page"
      );

      const titleEl = doc.querySelector(".title-text");
      const actualDataL10nID = titleEl.getAttribute("data-l10n-id");
      is(actualDataL10nID, l10nID, "Correct error page title is set");

      const expectedLabel =
        "Error code: " + expectedStatus.toString() + " " + expectedText;
      const actualLabel = doc.getElementById(
        "response-status-label"
      ).textContent;
      is(actualLabel, expectedLabel, "Correct response status message is set");
    }
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
}

add_task(async function test_blankPage_4xx() {
  await test_blankPage(BLANK_PAGE, "httpErrorPage-title", 400, "Bad Request");
});

add_task(async function test_blankPage_5xx() {
  await test_blankPage(
    BLANK_PAGE,
    "serverError-title",
    503,
    "Service Unavailable"
  );
});
