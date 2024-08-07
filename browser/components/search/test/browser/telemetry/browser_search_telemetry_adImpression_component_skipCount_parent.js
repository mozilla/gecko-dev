/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests skipCount property on parent elements.
 */

const TEST_PROVIDER_INFO = [
  {
    telemetryId: "example",
    searchPageRegexp: /^https:\/\/example.org\//,
    queryParamNames: ["s"],
    codeParamName: "abc",
    taggedCodes: ["ff"],
    adServerAttributes: ["mozAttr"],
    extraAdServersRegexps: [/^https:\/\/example\.com\/ad/],
  },
];

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

/**
 * This HTML contains non-sponsored links that wouldn't be picked up via a
 * bottom up search and thus, requires topdown inspection to find them.
 * It also contains sponsored links so that we do a bottom-up search.
 */
const TEST_URI = `
<!DOCTYPE html>
<main>
  <section class="refined-search-buttons">
      <a href="https://example.com/search?q=item+1">item 1</div>
      <a href="https://example.com/search?q=item+2">item 1</div>
  </section>
  <aside class="sidebar">
    <div class="sidebar-result">
      <a href="https://example.com/ad">Ad link</a>
    </div>
    <div class="sidebar-result">
      <a href="https://example.com/ad">Ad link</a>
    </div>
  </aside>
</main>
`;
const SERP_URL =
  "https://example.org/document-builder.sjs?s=test&abc=ff&html=" +
  encodeURIComponent(TEST_URI);

async function replaceIncludedProperty(topDown, bottomUp) {
  let components = [
    {
      type: SearchSERPTelemetryUtils.COMPONENTS.REFINED_SEARCH_BUTTONS,
      included: topDown,
      topDown: true,
    },
    {
      type: SearchSERPTelemetryUtils.COMPONENTS.AD_SIDEBAR,
      included: bottomUp,
    },
    {
      type: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
      default: true,
    },
  ];
  TEST_PROVIDER_INFO[0].components = components;
  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);
  await waitForIdle();
}

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

// For older clients, skipCount won't be available.
add_task(async function test_skip_count_not_provided() {
  await replaceIncludedProperty(
    {
      parent: {
        selector: ".refined-search-buttons",
      },
    },
    {
      parent: {
        selector: ".sidebar",
      },
    }
  );

  let { cleanup } = await openSerpInNewTab(SERP_URL);

  assertSERPTelemetry([
    {
      impression: IMPRESSION,
      adImpressions: [
        {
          component: SearchSERPTelemetryUtils.COMPONENTS.AD_SIDEBAR,
          ads_loaded: "1",
          ads_visible: "1",
          ads_hidden: "0",
        },
        {
          component: SearchSERPTelemetryUtils.COMPONENTS.REFINED_SEARCH_BUTTONS,
          ads_loaded: "1",
          ads_visible: "1",
          ads_hidden: "0",
        },
      ],
    },
  ]);

  await cleanup();
});

add_task(async function test_skip_count_is_false() {
  await replaceIncludedProperty(
    {
      parent: {
        selector: ".refined-search-buttons",
        skipCount: false,
      },
    },
    {
      parent: {
        selector: ".sidebar",
        skipCount: false,
      },
    }
  );

  let { cleanup } = await openSerpInNewTab(SERP_URL);

  assertSERPTelemetry([
    {
      impression: IMPRESSION,
      adImpressions: [
        {
          component: SearchSERPTelemetryUtils.COMPONENTS.AD_SIDEBAR,
          ads_loaded: "1",
          ads_visible: "1",
          ads_hidden: "0",
        },
        {
          component: SearchSERPTelemetryUtils.COMPONENTS.REFINED_SEARCH_BUTTONS,
          ads_loaded: "1",
          ads_visible: "1",
          ads_hidden: "0",
        },
      ],
    },
  ]);

  await cleanup();
});

add_task(async function test_skip_count_is_true() {
  await replaceIncludedProperty(
    {
      parent: {
        selector: ".refined-search-buttons",
        skipCount: true,
      },
    },
    {
      parent: {
        selector: ".sidebar",
        skipCount: true,
      },
    }
  );

  let { cleanup } = await openSerpInNewTab(SERP_URL);

  assertSERPTelemetry([
    {
      impression: IMPRESSION,
      adImpressions: [],
    },
  ]);

  await cleanup();
});
