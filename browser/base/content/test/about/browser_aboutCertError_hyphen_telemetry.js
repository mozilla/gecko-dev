/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const HYPHEN_LABEL_SITE = "https://hyphen-.example.com";
const DOMAIN_MISMATCH_SITE = "https://mismatch.badcertdomain.example.com";

registerCleanupFunction(async () => {
  await resetTelemetry();
});

async function resetTelemetry() {
  Services.telemetry.clearEvents();
  await TestUtils.waitForCondition(() => {
    let events = Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
      true
    ).content;
    return !events || !events.length;
  });
}

async function checkTelemetry(expectedHyphenCompat) {
  let loadEvent = await TestUtils.waitForCondition(() => {
    let events = Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
      true
    ).content;
    return events?.find(e => e[1] == "security.ui.certerror" && e[2] == "load");
  }, "recorded telemetry for the load");
  loadEvent.shift();
  Assert.deepEqual(loadEvent, [
    "security.ui.certerror",
    "load",
    "aboutcerterror",
    "SSL_ERROR_BAD_CERT_DOMAIN",
    {
      is_frame: "false",
      has_sts: "false",
      channel_status: "2153394164",
      issued_by_cca: "false",
      hyphen_compat: expectedHyphenCompat,
    },
  ]);
}

add_task(async function test_site_with_hyphen() {
  await resetTelemetry();
  let browser;
  let pageLoaded;
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    () => {
      gBrowser.selectedTab = BrowserTestUtils.addTab(
        gBrowser,
        HYPHEN_LABEL_SITE
      );
      browser = gBrowser.selectedBrowser;
      pageLoaded = BrowserTestUtils.waitForErrorPage(browser);
    },
    false
  );
  info("Loading and waiting for the certificate error page");
  await pageLoaded;
  // Check that telemetry indicates this error was caused by the hyphen in the
  // domain name.
  await checkTelemetry("true");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_domain_mismatch_site() {
  await resetTelemetry();
  let browser;
  let pageLoaded;
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    () => {
      gBrowser.selectedTab = BrowserTestUtils.addTab(
        gBrowser,
        DOMAIN_MISMATCH_SITE
      );
      browser = gBrowser.selectedBrowser;
      pageLoaded = BrowserTestUtils.waitForErrorPage(browser);
    },
    false
  );
  info("Loading and waiting for the certificate error page");
  await pageLoaded;
  // Check that telemetry indicates this error was not caused by a hyphen in
  // the domain name.
  await checkTelemetry("false");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
