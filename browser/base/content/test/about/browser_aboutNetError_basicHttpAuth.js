/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const AUTH_ROUTE =
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.com/browser/browser/base/content/test/about/basic_auth_route.sjs";

// From appstrings.properties
const EXPECTED_SHORT_DESC =
  "Someone pretending to be the site could try to steal things like your username, password, or email.";

add_task(async function test_basicHttpAuth() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // https first is disabled to enforce the scheme as http
      ["dom.security.https_first", false],
      ["network.http.basic_http_auth.enabled", false],
      // blank page with error is priortized
      ["browser.http.blank_page_with_error_response.enabled", true],
    ],
  });

  let browser;
  let pageLoaded;
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    () => {
      gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, AUTH_ROUTE);
      browser = gBrowser.selectedBrowser;
      pageLoaded = BrowserTestUtils.waitForErrorPage(browser);
    },
    false
  );

  info("Loading and waiting for the net error");
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
        "general-body-title",
        "Correct error page title is set"
      );

      // We use startsWith to account for the error code portion
      const shortDesc = doc.getElementById("errorShortDesc");
      ok(
        shortDesc.textContent.startsWith(expectedShortDesc),
        "Correct error page title is set"
      );

      const anchor = doc.querySelector("a");
      const actualAnchorl10nID = anchor.getAttribute("data-l10n-id");
      is(
        actualAnchorl10nID,
        "neterror-learn-more-link",
        "Correct error link is set"
      );
    }
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await SpecialPowers.popPrefEnv();
});
