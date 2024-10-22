/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const ISSUED_BY_CCA_SITE = "https://issued-by-cca.example.com";
const UNKNOWN_ISSUER_SITE = "https://untrusted.example.com";

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

async function checkTelemetry(expectedIssuedByCCA) {
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
    "SEC_ERROR_UNKNOWN_ISSUER",
    {
      is_frame: "false",
      has_sts: "false",
      channel_status: "2153390067",
      issued_by_cca: expectedIssuedByCCA,
      hyphen_compat: "false",
    },
  ]);
}

add_task(async function test_cca_site() {
  await resetTelemetry();
  let browser;
  let pageLoaded;
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    () => {
      gBrowser.selectedTab = BrowserTestUtils.addTab(
        gBrowser,
        ISSUED_BY_CCA_SITE
      );
      browser = gBrowser.selectedBrowser;
      pageLoaded = BrowserTestUtils.waitForErrorPage(browser);
    },
    false
  );
  info("Loading and waiting for the certificate error page");
  await pageLoaded;
  // Check that telemetry indicates this was issued by CCA.
  await checkTelemetry("true");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_non_cca_site() {
  await resetTelemetry();
  let browser;
  let pageLoaded;
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    () => {
      gBrowser.selectedTab = BrowserTestUtils.addTab(
        gBrowser,
        UNKNOWN_ISSUER_SITE
      );
      browser = gBrowser.selectedBrowser;
      pageLoaded = BrowserTestUtils.waitForErrorPage(browser);
    },
    false
  );
  info("Loading and waiting for the certificate error page");
  await pageLoaded;
  // Check that telemetry indicates this was not issued by CCA.
  await checkTelemetry("false");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
