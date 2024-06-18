/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This test specifically checks behavior of clicking on links that are
 * loaded after a page has been categorized for components.
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
    nonAdsLinkRegexps: [
      /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/telemetry\/searchTelemetryAd_nonAdsLink_redirect.html/,
    ],
    extraAdServersRegexps: [/^https:\/\/example\.com\/ad/],
    components: [
      {
        type: SearchSERPTelemetryUtils.COMPONENTS.AD_CAROUSEL,
        included: {
          parent: {
            selector: ".moz-carousel",
          },
          children: [
            {
              selector: ".moz-carousel-card",
              countChildren: true,
            },
          ],
          related: {
            selector: "button",
          },
        },
      },
      {
        type: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
        included: {
          parent: {
            selector: ".moz_ad",
          },
          children: [
            {
              selector: ".multi-col",
              type: SearchSERPTelemetryUtils.COMPONENTS.AD_SITELINK,
            },
          ],
          related: {
            selector: "button",
          },
        },
        excluded: {
          parent: {
            selector: ".rhs",
          },
        },
      },
      {
        type: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
        default: true,
      },
    ],
  },
];

// The impression and ad impression doesn't change across tests.
const IMPRESSION = {
  provider: "example",
  tagged: "true",
  partner_code: "ff",
  source: "unknown",
  is_shopping_page: "false",
  is_private: "false",
  shopping_tab_displayed: "false",
  is_signed_in: "false",
};

const AD_IMPRESSIONS = [
  {
    component: SearchSERPTelemetryUtils.COMPONENTS.AD_SITELINK,
    ads_loaded: "1",
    ads_visible: "1",
    ads_hidden: "0",
  },
  {
    component: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
    ads_loaded: "5",
    ads_visible: "5",
    ads_hidden: "0",
  },
];

add_setup(async function () {
  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);
  await waitForIdle();
  // Enable local telemetry recording for the duration of the tests.
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  registerCleanupFunction(async () => {
    SearchSERPTelemetry.overrideSearchTelemetryForTests();
    Services.telemetry.canRecordExtended = oldCanRecord;
    resetTelemetry();
  });
});

add_task(async function test_click_ad_created_after_page_load() {
  resetTelemetry();
  let url = getSERPUrl("searchTelemetryAd_components_text.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await waitForPageWithAdImpressions();

  info("Assert the page has had its components categorized.");
  assertSERPTelemetry([
    {
      impression: IMPRESSION,
      adImpressions: AD_IMPRESSIONS,
    },
  ]);

  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);

  info("Generate a sponsored link and click on it.");
  SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    let document = content.document;
    let el = document.createElement("div");
    let anchor = document.createElement("a");
    // This URL doesn't exist on the SERP.
    anchor.setAttribute("href", "https://example.com/ad/newpath");

    el.appendChild(anchor);
    content.document.getElementById("searchresults").appendChild(el);
    anchor.click();
  });

  await pageLoadPromise;

  assertSERPTelemetry([
    {
      impression: IMPRESSION,
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.AD_UNCATEGORIZED,
        },
      ],
      adImpressions: AD_IMPRESSIONS,
    },
  ]);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_click_non_ad_created_after_page_load() {
  resetTelemetry();
  let url = getSERPUrl("searchTelemetryAd_components_text.html");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await waitForPageWithAdImpressions();

  info("Assert the page has had its components categorized.");
  assertSERPTelemetry([
    {
      impression: IMPRESSION,
      adImpressions: AD_IMPRESSIONS,
    },
  ]);

  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);

  info("Generate a sponsored link and click on it.");
  SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    let document = content.document;
    let el = document.createElement("div");
    let anchor = document.createElement("a");
    // This URL doesn't exist on the SERP.
    anchor.setAttribute("href", "https://example.com/foo/bar");

    el.appendChild(anchor);
    content.document.getElementById("searchresults").appendChild(el);
    anchor.click();
  });

  await pageLoadPromise;

  assertSERPTelemetry([
    {
      impression: IMPRESSION,
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.NON_ADS_LINK,
        },
      ],
      adImpressions: AD_IMPRESSIONS,
    },
  ]);

  BrowserTestUtils.removeTab(tab);
});
