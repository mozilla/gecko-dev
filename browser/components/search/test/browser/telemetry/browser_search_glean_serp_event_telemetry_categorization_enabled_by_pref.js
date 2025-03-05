/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test to verify we can toggle the Glean SERP event telemetry for SERP
// categorization feature via a Nimbus variable.

const lazy = {};
const TELEMETRY_PREF =
  "browser.search.serpEventTelemetryCategorization.enabled";

ChromeUtils.defineESModuleGetters(lazy, {
  SERPDomainToCategoriesMap:
    "moz-src:///browser/components/search/SERPCategorization.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "serpEventsCategorizationEnabled",
  TELEMETRY_PREF,
  false
);

// This is required to trigger and properly categorize a SERP.
const TEST_PROVIDER_INFO = [
  {
    telemetryId: "example",
    searchPageRegexp:
      /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/telemetry\/searchTelemetry/,
    queryParamNames: ["s"],
    codeParamName: "abc",
    taggedCodes: ["ff"],
    adServerAttributes: ["mozAttr"],
    nonAdsLinkRegexps: [/^https:\/\/example.com/],
    extraAdServersRegexps: [/^https:\/\/example\.com\/ad/],
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

  // Enable local telemetry recording for the duration of the tests.
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  // If the categorization preference is enabled, we should also wait for the
  // sync event to update the domain to categories map.
  if (lazy.serpEventsCategorizationEnabled) {
    let promise = waitForDomainToCategoriesUpdate();
    await insertRecordIntoCollectionAndSync();
    await promise;
  } else {
    await insertRecordIntoCollectionAndSync();
  }

  registerCleanupFunction(async () => {
    Services.telemetry.canRecordExtended = oldCanRecord;
    await SpecialPowers.popPrefEnv();
    resetTelemetry();
  });
});

add_task(async function test_enable_experiment_when_pref_is_not_enabled() {
  let prefBranch = Services.prefs.getDefaultBranch("");
  let originalPrefValue = prefBranch.getBoolPref(TELEMETRY_PREF);

  // Ensure the build being tested has the preference value as false.
  // We do this on the default branch to simulate the value of the preference
  // being changed by the user.
  prefBranch.setBoolPref(TELEMETRY_PREF, false);

  // If it was true, we should wait until the map is fully un-inited.
  if (originalPrefValue) {
    await waitForDomainToCategoriesUninit();
  }

  Assert.equal(
    lazy.serpEventsCategorizationEnabled,
    false,
    "serpEventsCategorizationEnabled should be false when the default value is false."
  );

  info("Turn pref on");
  let updateComplete = waitForDomainToCategoriesUpdate();

  Services.prefs.setBoolPref(TELEMETRY_PREF, true);

  Assert.equal(
    lazy.serpEventsCategorizationEnabled,
    true,
    "serpEventsCategorizationEnabled should be true when the pref is true."
  );

  await updateComplete;

  let url = getSERPUrl("searchTelemetryDomainCategorizationReporting.html");
  info("Load a sample SERP with organic results.");
  let promise = waitForPageWithCategorizedDomains();
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await promise;

  await BrowserTestUtils.removeTab(tab);
  assertCategorizationValues([
    {
      organic_category: "3",
      organic_num_domains: "1",
      organic_num_inconclusive: "0",
      organic_num_unknown: "0",
      sponsored_category: "4",
      sponsored_num_domains: "2",
      sponsored_num_inconclusive: "0",
      sponsored_num_unknown: "0",
      mappings_version: "1",
      app_version: APP_MAJOR_VERSION,
      channel: CHANNEL,
      region: REGION,
      partner_code: "ff",
      provider: "example",
      tagged: "true",
      is_shopping_page: "false",
      num_ads_clicked: "0",
      num_ads_hidden: "0",
      num_ads_loaded: "2",
      num_ads_visible: "2",
    },
  ]);
  resetTelemetry();

  info("Turn pref off");
  Services.prefs.setBoolPref(TELEMETRY_PREF, false);
  await waitForDomainToCategoriesUninit();

  Assert.equal(
    lazy.serpEventsCategorizationEnabled,
    false,
    "serpEventsCategorizationEnabled should be false when the pref is false."
  );

  Assert.ok(
    lazy.SERPDomainToCategoriesMap.empty,
    "Domain to categories map should be empty."
  );

  info("Load a sample SERP with organic results.");
  tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  // Wait an arbitrary amount for a possible categorization.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 1500));
  BrowserTestUtils.removeTab(tab);

  // We should not record telemetry if the experiment is un-enrolled.
  assertCategorizationValues([]);

  // Clean up.
  prefBranch.setBoolPref(TELEMETRY_PREF, originalPrefValue);
});
