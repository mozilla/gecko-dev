/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const SERVER_ERROR_PAGE =
  "https://example.com/browser/browser/base/content/test/about/server_error.sjs";

add_task(async function test_serverError() {
  let browser;
  let pageLoaded;
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    () => {
      gBrowser.selectedTab = BrowserTestUtils.addTab(
        gBrowser,
        SERVER_ERROR_PAGE
      );
      browser = gBrowser.selectedBrowser;
      pageLoaded = BrowserTestUtils.waitForErrorPage(browser);
    },
    false
  );

  info("Loading and waiting for the net error");
  await pageLoaded;

  await SpecialPowers.spawn(browser, [], function () {
    const doc = content.document;
    ok(
      doc.documentURI.startsWith("about:neterror"),
      "Should be showing error page"
    );

    const titleEl = doc.querySelector(".title-text");
    const actualDataL10nID = titleEl.getAttribute("data-l10n-id");
    is(
      actualDataL10nID,
      "serverError-title",
      "Correct error page title is set"
    );
    const responseStatusLabel = doc.getElementById(
      "response-status-label"
    ).textContent;
    is(
      responseStatusLabel,
      "Error code: 500 Internal Server Error",
      "Correct response status message is set"
    );
  });

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
