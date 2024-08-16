/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the `fakespot_engagement` event.

const FAKESPOT_URL = "https://example.com/fakespot";
const AMP_URL = "https://example.com/amp";

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
            url: FAKESPOT_URL,
            title: "Fakespot suggestion",
            rating: 4.8,
            total_reviews: 1234567,
            fakespot_grade: "A",
            product_id: "amazon-0",
            score: 0.01,
            keywords: "",
            product_type: "",
          },
        ],
      },
      {
        type: "data",
        // eslint-disable-next-line mozilla/valid-lazy
        attachment: [lazy.QuickSuggestTestUtils.ampRemoteSettings()],
      },
    ],
    prefs: [["fakespot.featureGate", true]],
  });

  registerCleanupFunction(async () => {
    await cleanupQuickSuggest();
    Services.fog.testResetFOG();
  });
});

// Clicks a Fakespot suggestion.
add_task(async function engagement_fakespot() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "fakespot",
    });

    Assert.ok(
      await getRowByURL(FAKESPOT_URL),
      "Fakespot row should be present"
    );

    await selectRowByURL(FAKESPOT_URL);
    await doEnter();

    assertGleanTelemetry("fakespot_engagement", [
      { grade: "A", rating: "4.8", provider: "amazon" },
    ]);
  });

  Services.fog.testResetFOG();
});

// Clicks a non-Fakespot suggestion without matching Fakespot.
add_task(async function engagement_other_fakespotAbsent() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "amp",
    });

    Assert.ok(
      !(await getRowByURL(FAKESPOT_URL)),
      "Fakespot row should be absent"
    );
    Assert.ok(await getRowByURL(AMP_URL), "AMP row should be present");

    await selectRowByURL(AMP_URL);
    await doEnter();

    assertGleanTelemetry("fakespot_engagement", []);
  });

  Services.fog.testResetFOG();
});

// Clicks a non-Fakespot suggestion after also matching Fakespot.
add_task(async function engagement_other_fakespotPresent() {
  let visitUrl = "https://example.com/some-other-suggestion";
  await PlacesTestUtils.addVisits([visitUrl]);

  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "suggestion",
    });

    Assert.ok(
      await getRowByURL(FAKESPOT_URL),
      "Fakespot row should be present"
    );
    Assert.ok(
      await getRowByURL(visitUrl),
      "Visit/history row should be present"
    );

    await selectRowByURL(visitUrl);
    await doEnter();

    assertGleanTelemetry("fakespot_engagement", []);
  });

  Services.fog.testResetFOG();
});

// Abandons the search session after after matching Fakespot.
add_task(async function abandonment() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "fakespot",
    });

    Assert.ok(
      await getRowByURL(FAKESPOT_URL),
      "Fakespot row should be present"
    );

    await doBlur();

    assertGleanTelemetry("fakespot_engagement", []);
  });

  Services.fog.testResetFOG();
});

async function getRowByURL(url) {
  for (let i = 0; i < UrlbarTestUtils.getResultCount(window); i++) {
    const detail = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    if (detail.url === url) {
      return detail;
    }
  }
  return null;
}
