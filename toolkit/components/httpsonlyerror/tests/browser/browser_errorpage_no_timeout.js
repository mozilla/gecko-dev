/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// We need to request longer timeout because HTTPS-Only Mode sends the
// backround http request with a delay of N milliseconds before the
// actual load gets cancelled.
requestLongerTimeout(2);

const TEST_PATH_HTTP = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  // This needs to be insecure so it can be upgraded
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.com"
);
const TEST_PATH_HTTPS = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

const HSTS_SITE = TEST_PATH_HTTPS + "file_https_telemetry_hsts.sjs";

const TIMEOUT_PAGE_URI_HTTP =
  TEST_PATH_HTTP + "file_errorpage_no_timeout_server.sjs";
const TIMEOUT_PAGE_URI_HTTPS =
  TEST_PATH_HTTPS + "file_errorpage_no_timeout_server.sjs";

async function runUpgradeTest(aURI, aDesc, aAssertURLStartsWith) {
  await BrowserTestUtils.withNewTab("about:blank", async function (browser) {
    const loaded = BrowserTestUtils.browserLoaded(browser, false, null, true);
    BrowserTestUtils.startLoadingURIString(browser, aURI);
    await loaded;

    await SpecialPowers.spawn(
      browser,
      [aDesc, aAssertURLStartsWith],
      async function (aDesc, aAssertURLStartsWith) {
        ok(
          content.document.location.href.startsWith(aAssertURLStartsWith),
          aDesc
        );
      }
    );
    await SpecialPowers.removePermission("https-only-load-insecure", aURI);
  });
}

async function getPage(wanted) {
  await BrowserTestUtils.withNewTab("about:blank", async function (browser) {
    info(`wanted: ${wanted}`);
    let loaded = BrowserTestUtils.browserLoaded(
      browser,
      false, // includeSubFrames = false, no need to includeSubFrames
      TIMEOUT_PAGE_URI_HTTPS,
      true // maybeErrorPage = true, because we need the error page to appear
    );

    BrowserTestUtils.startLoadingURIString(browser, TIMEOUT_PAGE_URI_HTTP);
    await loaded;

    await SpecialPowers.spawn(browser, [wanted], async function (wanted) {
      const doc = content.document;
      let errorPage = doc.body.innerHTML;
      ok(
        errorPage.includes(wanted),
        `Response must contain search string: ${wanted}`
      );
    });
  });
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Reduce dom.security.https_only_fire_http_request_background_timer_ms to 1s
      ["dom.security.https_only_fire_http_request_background_timer_ms", 1000],
      // If only the user wants upgrades, we offer fallbacks.
      // But even if the user wants upgrades, we shouldn't fallback if HSTS is on.
      ["dom.security.https_only_mode", true],
      ["dom.security.https_first", true],
      ["dom.security.https_first_schemeless", true],
    ],
  });
});

add_task(async function wait_longer_for_hsts_page() {
  // Without HSTS we should be timing out and showing the HTTPS-only error page.
  await getPage("about-httpsonly-title-site-not-available");

  // Set up HSTS by querying a site which returns the header
  await BrowserTestUtils.withNewTab("about:blank", async function (browser) {
    const loaded = BrowserTestUtils.browserLoaded(browser, false, null, true);
    BrowserTestUtils.startLoadingURIString(browser, HSTS_SITE);
    await loaded;
  });

  // With HSTS enabled we should be waiting for the secure site to load.
  await getPage("slow");

  // Disable HSTS again!
  const sss = Cc["@mozilla.org/ssservice;1"].getService(
    Ci.nsISiteSecurityService
  );
  sss.clearAll();
});
