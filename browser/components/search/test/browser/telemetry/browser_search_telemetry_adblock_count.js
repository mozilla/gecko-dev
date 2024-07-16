/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BUILDER_URL = "https://example.com/document-builder.sjs?html=";

/**
 * This HTML file contains three ads:
 * - An ad that is well above the possible viewport.
 * - The numbers of ads are unique to help ensure counts are correct.
 */
const TEST_URI = `
<!DOCTYPE html>
<main>
  <style>
    .ad_parent {
      display: none;
    }
    /*
      This is if the ad blocker doesn't block the parent component but
      instead blocks the child.
    */
    .ad_with_children div {
      display: none;
    }
    .ad_far_above {
      position: absolute;
      top: -9999px;
    }
  </style>
  <div class="ad_far_above">
    <a href="https://example.com/ad">Ad link</a>
  </div>
  <div class="ad_parent">
    <a href="https://example.com/ad">Ad link</a>
  </div>
  <div class="ad_parent">
    <a href="https://example.com/ad">Ad link</a>
  </div>
  <div class="ad_with_children">
    <span>Element</span>
    <div class="child">
      <a href="https://example.com/ad">Ad link</a>
    </div>
    <div class="child">
      <a href="https://example.com/ad">Ad link</a>
    </div>
    <div class="child">
      <a href="https://example.com/ad">Ad link</a>
    </div>
  </div>
</main>
`;
const URL =
  "https://example.org/document-builder.sjs?html=" +
  encodeURIComponent(TEST_URI) +
  "&s=foobar&abc=ff";

const TEST_PROVIDER_INFO = [
  {
    telemetryId: "example",
    searchPageRegexp: /^https:\/\/example\.org\/document-builder\.sjs/,
    queryParamNames: ["s"],
    codeParamName: "abc",
    taggedCodes: ["ff"],
    extraAdServersRegexps: [/^https:\/\/example\.com\/ad/],
    components: [
      {
        type: SearchSERPTelemetryUtils.COMPONENTS.AD_CAROUSEL,
        included: {
          parent: {
            selector: ".ad_with_children",
          },
          children: [
            {
              selector: ".child",
              countChildren: true,
            },
          ],
        },
      },
      {
        type: SearchSERPTelemetryUtils.COMPONENTS.AD_SITELINK,
        included: {
          parent: {
            selector: ".ad_parent",
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

add_task(async function test_adblock_count() {
  let { cleanup } = await openSerpInNewTab(URL);

  assertSERPTelemetry([
    {
      impression: {
        is_signed_in: "false",
        is_private: "false",
        source: "unknown",
        is_shopping_page: "false",
        partner_code: "ff",
        provider: "example",
        shopping_tab_displayed: "false",
        tagged: "true",
      },
      adImpressions: [
        {
          component: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
          ads_loaded: "1",
          ads_visible: "0",
          ads_hidden: "1",
        },
        {
          component: SearchSERPTelemetryUtils.COMPONENTS.AD_SITELINK,
          ads_loaded: "2",
          ads_visible: "0",
          ads_hidden: "2",
        },
        {
          component: SearchSERPTelemetryUtils.COMPONENTS.AD_CAROUSEL,
          ads_loaded: "3",
          ads_visible: "0",
          ads_hidden: "3",
        },
      ],
    },
  ]);

  await Services.fog.testFlushAllChildren();

  Assert.equal(
    1,
    Glean.serp.adsBlockedCount.beyond_viewport.testGetValue(),
    "Number of ads blocked due to being beyond the viewport."
  );
  Assert.equal(
    2,
    Glean.serp.adsBlockedCount.hidden_parent.testGetValue(),
    "Number of parent elements blocked."
  );
  Assert.equal(
    3,
    Glean.serp.adsBlockedCount.hidden_child.testGetValue(),
    "Number of child elements blocked."
  );

  await cleanup();
});
