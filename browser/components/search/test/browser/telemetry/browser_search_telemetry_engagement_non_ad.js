/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests load SERPs and click on links that are non ads. Non ads can have
 * slightly different behavior from ads.
 */

"use strict";

const TEST_PROVIDER_INFO = [
  {
    telemetryId: "example",
    searchPageRegexp:
      /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/telemetry\/searchTelemetryAd_/,
    queryParamNames: ["s"],
    codeParamName: "abc",
    taggedCodes: ["ff"],
    adServerAttributes: ["mozAttr"],
    nonAdsLinkRegexps: [],
    extraAdServersRegexps: [/^https:\/\/example\.com\/ad/],
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

  registerCleanupFunction(async () => {
    SearchSERPTelemetry.overrideSearchTelemetryForTests();
    resetTelemetry();
  });
});

// Click on an non-ad element while no ads are present. The non-ad is also
// not included in nonAdsLinkRegexps.
add_task(async function test_click_non_ad_with_no_ads() {
  await waitForIdle();

  resetTelemetry();

  let url = getSERPUrl("searchTelemetryAd_searchbox.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await waitForPageWithAdImpressions();

  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    true,
    "https://example.com/hello_world"
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#non_ads_link",
    {},
    tab.linkedBrowser
  );
  await browserLoadedPromise;

  assertSERPTelemetry([
    {
      impression: {
        provider: "example",
        tagged: "true",
        partner_code: "ff",
        source: "unknown",
        is_shopping_page: "false",
        is_private: "false",
        shopping_tab_displayed: "false",
        is_signed_in: "false",
      },
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.NON_ADS_LINK,
        },
      ],
    },
  ]);

  BrowserTestUtils.removeTab(tab);

  // Reset state for other tests.
  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);
  await waitForIdle();
});
