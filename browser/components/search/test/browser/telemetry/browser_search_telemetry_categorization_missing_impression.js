/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
 * This test verifies that we correctly record telemetry when issues with the
 * domain-to-categories map cause the categorization and impression events
 * not to be recorded.
 */

const TEST_PROVIDER_INFO = [
  {
    telemetryId: "example",
    searchPageRegexp:
      /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/telemetry\/searchTelemetry/,
    queryParamNames: ["s"],
    codeParamName: "abc",
    taggedCodes: ["ff"],
    adServerAttributes: ["mozAttr"],
    nonAdsLinkRegexps: [],
    extraAdServersRegexps: [/^https:\/\/example\.com\/ad/],
    // The search telemetry entry responsible for targeting the specific results.
    domainExtraction: {
      ads: [
        {
          selectors: "[data-ad-domain]",
          method: "dataAttribute",
          options: {
            dataAttributeKey: "adDomain",
          },
        },
        {
          selectors: ".ad",
          method: "href",
          options: {
            queryParamKey: "ad_domain",
          },
        },
      ],
      nonAds: [
        {
          selectors: "#results .organic a",
          method: "href",
        },
      ],
    },
    components: [
      {
        type: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
        default: true,
      },
    ],
  },
];

add_setup(async function () {
  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);
  await waitForIdle();

  let promise = waitForDomainToCategoriesUpdate();
  await insertRecordIntoCollectionAndSync();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.serpEventTelemetryCategorization.enabled", true],
      ["browser.search.serpMetricsRecordedCounter", 0],
    ],
  });
  await promise;

  registerCleanupFunction(async () => {
    // Manually unload the pref so that we can check if we should wait for the
    // the categories map to be un-initialized.
    await SpecialPowers.popPrefEnv();
    if (
      !Services.prefs.getBoolPref(
        "browser.search.serpEventTelemetryCategorization.enabled"
      )
    ) {
      await waitForDomainToCategoriesUninit();
    }
    SearchSERPTelemetry.overrideSearchTelemetryForTests();
    resetTelemetry();
  });
});

add_task(async function test_count_not_incremented_if_map_is_ok() {
  let url = getSERPUrl("searchTelemetryDomainCategorizationReporting.html");
  info("Load a SERP with organic and sponsored results.");
  let promise = waitForPageWithCategorizedDomains();
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await promise;

  info(
    "Domain-to-categories map should already be downloaded and populated at this point."
  );

  Assert.equal(
    Glean.serp.categorizationNoMapFound.testGetValue(),
    null,
    "Counter should be null when there are no issues with the domain-to-categories map."
  );

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_count_incremented_if_map_is_not_downloaded() {
  resetTelemetry();

  // Clear the existing domain-to-categories map.
  await SearchSERPDomainToCategoriesMap.uninit({ deleteMap: true });

  let sandbox = sinon.createSandbox();
  sandbox
    .stub(RemoteSettings(TELEMETRY_CATEGORIZATION_KEY).attachments, "download")
    .throws(new Error());
  let downloadError = TestUtils.consoleMessageObserved(msg => {
    return (
      typeof msg.wrappedJSObject.arguments?.[0] == "string" &&
      msg.wrappedJSObject.arguments[0].includes("Could not download file:")
    );
  });
  await SearchSERPDomainToCategoriesMap.init();
  info("Wait for download error.");
  await downloadError;
  info("Domain-to-categories map unsuccessfully downloaded.");

  let url = getSERPUrl("searchTelemetryDomainCategorizationReporting.html");
  info("Load a SERP with organic and sponsored results.");
  let promise = waitForPageWithCategorizedDomains();
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await promise;

  Assert.equal(
    Glean.serp.categorizationNoMapFound.testGetValue(),
    1,
    "Counter should be incremented when there is an issue downloading the domain-to-categories map."
  );

  sandbox.restore();
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_threshold_reached() {
  resetTelemetry();

  let oldThreshold = CATEGORIZATION_SETTINGS.PING_SUBMISSION_THRESHOLD;
  // For testing, it's fine to categorize fewer SERPs before sending the ping.
  CATEGORIZATION_SETTINGS.PING_SUBMISSION_THRESHOLD = 1;
  SERPCategorizationRecorder.uninit();
  SERPCategorizationRecorder.init();

  Assert.equal(
    null,
    Glean.serp.categorizationNoMapFound.testGetValue(),
    "Should not have recorded any metrics yet."
  );

  // Simulate a broken domain-to-categories map.
  let sandbox = sinon.createSandbox();
  sandbox.stub(SearchSERPDomainToCategoriesMap, "get").returns([]);

  let submitted = false;
  GleanPings.serpCategorization.testBeforeNextSubmit(reason => {
    submitted = true;
    Assert.equal(
      1,
      Glean.serp.categorizationNoMapFound.testGetValue(),
      "Should record one missing impression due to a broken domain-to-categories map."
    );

    Assert.equal(
      "threshold_reached",
      reason,
      "Ping submission reason should be 'threshold_reached'."
    );
  });

  let url = getSERPUrl("searchTelemetryDomainCategorizationReporting.html");
  info("Load a sample SERP with organic and sponsored results.");
  let promise = waitForPageWithCategorizedDomains();
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await promise;

  BrowserTestUtils.removeTab(tab);

  Assert.equal(
    true,
    submitted,
    "Ping should be submitted once threshold is reached."
  );

  CATEGORIZATION_SETTINGS.PING_SUBMISSION_THRESHOLD = oldThreshold;
  sandbox.restore();
});
