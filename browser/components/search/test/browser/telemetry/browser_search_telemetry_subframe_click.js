/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests check sponsored links in subframes are reported accurately when
 * clicked. For ease of testing with subframes, we use the same domain for both
 * the parent and subframe to avoid cross domain issues.
 */

"use strict";

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
    subframes: [
      {
        regexp:
          /^https:\/\/example\.org\/browser\/browser\/components\/search\/test\/browser\/telemetry/,
        inspectRegexpInSERP: true,
        inspectRegexpInParent: true,
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

add_task(async function test_click_subframe_link_and_load_page() {
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

  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(
    gBrowser,
    "https://example.com/ad"
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    const doc = content.document;
    const iframe = doc.querySelector("iframe");
    const iframeDocument = iframe.contentWindow.document;
    iframeDocument.querySelector("a#open_in_same_tab").click();
  });
  await pageLoadPromise;

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example:tagged:ff": 1 },
      "browser.search.withads.unknown": { "example:tagged": 1 },
      "browser.search.adclicks.unknown": { "example:tagged": 1 },
    }
  );

  BrowserTestUtils.removeTab(tab);
  resetTelemetry();
});

add_task(async function test_click_subframe_link_in_new_tab() {
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

  let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);
  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(
    gBrowser,
    "https://example.com/ad"
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    const doc = content.document;
    const iframe = doc.querySelector("iframe");
    const iframeDocument = iframe.contentWindow.document;
    iframeDocument.querySelector("a#open_in_new_tab").click();
  });
  let newTab = await newTabPromise;
  await pageLoadPromise;

  BrowserTestUtils.removeTab(newTab);

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example:tagged:ff": 1 },
      "browser.search.withads.unknown": { "example:tagged": 1 },
      "browser.search.adclicks.unknown": { "example:tagged": 1 },
    }
  );

  BrowserTestUtils.removeTab(tab);
  resetTelemetry();
});

add_task(async function test_click_subframe_link_in_new_window() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.link.open_newwindow", 2]],
  });

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

  let newWindowPromise = BrowserTestUtils.waitForNewWindow({
    url: "https://example.com/ad",
    maybeErrorPage: true,
  });
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    const doc = content.document;
    const iframe = doc.querySelector("iframe");
    const iframeDocument = iframe.contentWindow.document;
    iframeDocument.querySelector("a#open_in_new_tab").click();
  });
  let newWindow = await newWindowPromise;

  await assertSearchSourcesTelemetry(
    {},
    {
      "browser.search.content.unknown": { "example:tagged:ff": 1 },
      "browser.search.withads.unknown": { "example:tagged": 1 },
      "browser.search.adclicks.unknown": { "example:tagged": 1 },
    }
  );

  await SpecialPowers.popPrefEnv();
  newWindow.close();
  BrowserTestUtils.removeTab(tab);
  resetTelemetry();
});
