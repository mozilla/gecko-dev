/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests check sponsored links in subframes are reported as found on pages.
 */

"use strict";

// Just in case waiting for the non-presence of ads causes failures on TV mode.
requestLongerTimeout(2);

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
    extraAdServersRegexps: [/^http:\/\/localhost\:40000\/ad/],
    subframes: [
      {
        regexp:
          /^https:\/\/test1\.example\.com\/browser\/browser\/components\/search\/test\/browser\/telemetry\/searchTelemetrySubframe/,
        inspectRegexpInSERP: true,
      },
    ],
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

add_task(async function test_sponsored_subframe_visible() {
  info("Load SERP in a new tab.");
  let url = getSERPUrl(
    "searchTelemetryAd_with_sponsored_subframe_visible.html"
  );
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example:tagged:ff": 1 },
      "browser.search.withads.unknown": { "example:tagged": 1 },
    }
  );

  BrowserTestUtils.removeTab(tab);
  resetTelemetry();
});

add_task(async function test_sponsored_subframe_hidden() {
  info("Load SERP in a new tab.");
  let url = getSERPUrl("searchTelemetryAd_with_sponsored_subframe_hidden.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example:tagged:ff": 1 },
    }
  );

  BrowserTestUtils.removeTab(tab);
  resetTelemetry();
});

add_task(async function test_sponsored_subframe_no_regexp_inspection() {
  let NO_REGEXP_INSPECTION = [
    {
      ...TEST_PROVIDER_INFO[0],
      subframes: [
        {
          regexp: TEST_PROVIDER_INFO[0].subframes[0].regexp,
          inspectRegexpInSERP: false,
        },
      ],
    },
  ];

  SearchSERPTelemetry.overrideSearchTelemetryForTests(NO_REGEXP_INSPECTION);
  await waitForIdle();

  info("Load SERP in a new tab.");
  let url = getSERPUrl(
    "searchTelemetryAd_with_sponsored_subframe_visible.html"
  );
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example:tagged:ff": 1 },
    }
  );

  info("Wait for ads to potentially be recorded in telemery.");
  /* eslint-disable-next-line mozilla/no-arbitrary-setTimeout */
  await new Promise(resolve => setTimeout(resolve, 1000));

  let scalars = Services.telemetry.getSnapshotForKeyedScalars("main", false);
  Assert.ok(
    !scalars.parent.hasOwnProperty("browser.search.withads.unknown"),
    "Ads weren't found on page."
  );

  BrowserTestUtils.removeTab(tab);
  resetTelemetry();
});
