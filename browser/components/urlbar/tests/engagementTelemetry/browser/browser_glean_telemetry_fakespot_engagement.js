/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the `fakespot_engagement` event.

add_setup(async function test_setup() {
  Services.fog.testResetFOG();

  // Add a mock engine so we don't hit the network.
  await SearchTestUtils.installSearchExtension({}, { setAsDefault: true });

  let cleanupQuickSuggest = await ensureQuickSuggestInit({
    remoteSettingsRecords: [
      {
        collection: "fakespot-suggest-products",
        type: "fakespot-suggestions",
        attachment: [
          {
            url: "https://example.com/maybe-good-item",
            title: "Maybe Good Item",
            rating: 4.8,
            total_reviews: 1234567,
            fakespot_grade: "A",
            product_id: "amazon-0",
            score: 0.01,
          },
        ],
      },
      {
        type: "data",
        attachment: [
          {
            id: 1,
            url: "https://example.com/sponsored",
            title: "Sponsored suggestion",
            keywords: ["sponsored"],
            click_url: "https://example.com/click",
            impression_url: "https://example.com/impression",
            advertiser: "TestAdvertiser",
            iab_category: "22 - Shopping",
            icon: "1234",
          },
        ],
      },
    ],
    prefs: [["fakespot.featureGate", true]],
  });

  registerCleanupFunction(async () => {
    await cleanupQuickSuggest();
    Services.fog.testResetFOG();
  });
});

add_task(async function engagement_fakespot() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "maybe",
    });

    await selectRowByProvider("UrlbarProviderQuickSuggest");
    await doEnter();

    assertGleanTelemetry("fakespot_engagement", [
      { grade: "A", rating: "4.8", provider: "amazon" },
    ]);
  });

  Services.fog.testResetFOG();
});

add_task(async function engagement_notFakespot() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "sponsored",
    });

    await selectRowByURL("https://example.com/sponsored");
    await doEnter();

    assertGleanTelemetry("fakespot_engagement", []);
  });

  Services.fog.testResetFOG();
});

add_task(async function abandonment() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "maybe",
    });

    await selectRowByProvider("UrlbarProviderQuickSuggest");
    await doBlur();

    assertGleanTelemetry("fakespot_engagement", []);
  });

  Services.fog.testResetFOG();
});
