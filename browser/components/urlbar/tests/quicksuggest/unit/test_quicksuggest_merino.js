/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests Merino integration with UrlbarProviderQuickSuggest.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  AmpSuggestions: "resource:///modules/urlbar/private/AmpSuggestions.sys.mjs",
});

// relative to `browser.urlbar`
const PREF_DATA_COLLECTION_ENABLED = "quicksuggest.dataCollection.enabled";

const SEARCH_STRING = "frab";

const { DEFAULT_SUGGESTION_SCORE } = UrlbarProviderQuickSuggest;
const { TIMESTAMP_TEMPLATE } = AmpSuggestions;

const REMOTE_SETTINGS_RESULTS = [
  QuickSuggestTestUtils.ampRemoteSettings({
    keywords: [SEARCH_STRING],
  }),
];

const EXPECTED_REMOTE_SETTINGS_URLBAR_RESULT = QuickSuggestTestUtils.ampResult({
  keyword: SEARCH_STRING,
});

const EXPECTED_MERINO_URLBAR_RESULT = QuickSuggestTestUtils.ampResult({
  source: "merino",
  provider: "adm",
  requestId: "request_id",
});

add_setup(async () => {
  await MerinoTestUtils.server.start();

  // Set up the remote settings client with the test data.
  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    prefs: [
      ["suggest.quicksuggest.nonsponsored", true],
      ["suggest.quicksuggest.sponsored", true],
    ],
  });
  await resetRemoteSettingsData();

  Assert.equal(
    typeof DEFAULT_SUGGESTION_SCORE,
    "number",
    "Sanity check: DEFAULT_SUGGESTION_SCORE is defined"
  );
});

// Tests with the Merino endpoint URL set to an empty string, which disables
// fetching from Merino.
add_task(async function merinoDisabled() {
  let mockEndpointUrl = UrlbarPrefs.get("merino.endpointURL");
  UrlbarPrefs.set("merino.endpointURL", "");
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  // Clear the remote settings suggestions so that if Merino is actually queried
  // -- which would be a bug -- we don't accidentally mask the Merino suggestion
  // by also matching an RS suggestion with the same or higher score.
  await QuickSuggestTestUtils.setRemoteSettingsRecords([]);

  let context = createContext(SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [],
  });

  UrlbarPrefs.set("merino.endpointURL", mockEndpointUrl);

  await resetRemoteSettingsData();
});

// Tests with Merino enabled but with data collection disabled. Results should
// not be fetched from Merino in that case.
add_task(async function dataCollectionDisabled() {
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, false);

  // Clear the remote settings suggestions so that if Merino is actually queried
  // -- which would be a bug -- we don't accidentally mask the Merino suggestion
  // by also matching an RS suggestion with the same or higher score.
  await QuickSuggestTestUtils.setRemoteSettingsRecords([]);

  let context = createContext(SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [],
  });

  await resetRemoteSettingsData();
});

// When the Merino suggestion has a higher score than the remote settings
// suggestion, the Merino suggestion should be used.
add_task(async function higherScore() {
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  MerinoTestUtils.server.response.body.suggestions[0].score =
    2 * DEFAULT_SUGGESTION_SCORE;

  let context = createContext(SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_MERINO_URLBAR_RESULT],
  });

  MerinoTestUtils.server.reset();
  merinoClient().resetSession();
});

// When the Merino suggestion has a lower score than the remote settings
// suggestion, the remote settings suggestion should be used.
add_task(async function lowerScore() {
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  MerinoTestUtils.server.response.body.suggestions[0].score =
    DEFAULT_SUGGESTION_SCORE / 2;

  let context = createContext(SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_REMOTE_SETTINGS_URLBAR_RESULT],
  });

  MerinoTestUtils.server.reset();
  merinoClient().resetSession();
});

// When remote settings doesn't return a suggestion but Merino does, the Merino
// suggestion should be used.
add_task(async function noSuggestion_remoteSettings() {
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  let context = createContext("this doesn't match remote settings", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_MERINO_URLBAR_RESULT],
  });

  MerinoTestUtils.server.reset();
  merinoClient().resetSession();
});

// When Merino doesn't return a suggestion but remote settings does, the remote
// settings suggestion should be used.
add_task(async function noSuggestion_merino() {
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  MerinoTestUtils.server.response.body.suggestions = [];

  let context = createContext(SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_REMOTE_SETTINGS_URLBAR_RESULT],
  });

  MerinoTestUtils.server.reset();
  merinoClient().resetSession();
});

// When Merino returns multiple suggestions, the one with the largest score
// should be used.
add_task(async function multipleMerinoSuggestions() {
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  MerinoTestUtils.server.response.body.suggestions = [
    {
      provider: "adm",
      full_keyword: "multipleMerinoSuggestions 0 full_keyword",
      title: "multipleMerinoSuggestions 0 title",
      url: "multipleMerinoSuggestions 0 url",
      icon: "multipleMerinoSuggestions 0 icon",
      impression_url: "multipleMerinoSuggestions 0 impression_url",
      click_url: "multipleMerinoSuggestions 0 click_url",
      block_id: 0,
      advertiser: "multipleMerinoSuggestions 0 advertiser",
      iab_category: "22 - Shopping",
      is_sponsored: true,
      score: 0.1,
    },
    {
      provider: "adm",
      full_keyword: "multipleMerinoSuggestions 1 full_keyword",
      title: "multipleMerinoSuggestions 1 title",
      url: "multipleMerinoSuggestions 1 url",
      icon: "multipleMerinoSuggestions 1 icon",
      impression_url: "multipleMerinoSuggestions 1 impression_url",
      click_url: "multipleMerinoSuggestions 1 click_url",
      block_id: 1,
      advertiser: "multipleMerinoSuggestions 1 advertiser",
      iab_category: "22 - Shopping",
      is_sponsored: true,
      score: 1,
    },
    {
      provider: "adm",
      full_keyword: "multipleMerinoSuggestions 2 full_keyword",
      title: "multipleMerinoSuggestions 2 title",
      url: "multipleMerinoSuggestions 2 url",
      icon: "multipleMerinoSuggestions 2 icon",
      impression_url: "multipleMerinoSuggestions 2 impression_url",
      click_url: "multipleMerinoSuggestions 2 click_url",
      block_id: 2,
      advertiser: "multipleMerinoSuggestions 2 advertiser",
      iab_category: "22 - Shopping",
      is_sponsored: true,
      score: 0.2,
    },
  ];

  let context = createContext("test", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      QuickSuggestTestUtils.ampResult({
        keyword: "multipleMerinoSuggestions 1 full_keyword",
        title: "multipleMerinoSuggestions 1 title",
        url: "multipleMerinoSuggestions 1 url",
        originalUrl: "multipleMerinoSuggestions 1 url",
        icon: "multipleMerinoSuggestions 1 icon",
        impressionUrl: "multipleMerinoSuggestions 1 impression_url",
        clickUrl: "multipleMerinoSuggestions 1 click_url",
        blockId: 1,
        advertiser: "multipleMerinoSuggestions 1 advertiser",
        requestId: "request_id",
        source: "merino",
        provider: "adm",
      }),
    ],
  });

  MerinoTestUtils.server.reset();
  merinoClient().resetSession();
});

// Timestamp templates in URLs should be replaced with real timestamps.
add_task(async function timestamps() {
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  // Set up the Merino response with template URLs.
  let suggestion = MerinoTestUtils.server.response.body.suggestions[0];
  suggestion.url = `http://example.com/time-${TIMESTAMP_TEMPLATE}`;
  suggestion.click_url = `http://example.com/time-${TIMESTAMP_TEMPLATE}-foo`;

  // Do a search.
  let context = createContext("test", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  let controller = UrlbarTestUtils.newMockController({
    input: {
      isPrivate: context.isPrivate,
      onFirstResult() {
        return false;
      },
      getSearchSource() {
        return "dummy-search-source";
      },
      window: {
        location: {
          href: AppConstants.BROWSER_CHROME_URL,
        },
      },
    },
  });
  await controller.startQuery(context);

  // Should be one quick suggest result.
  Assert.equal(context.results.length, 1, "One result returned");
  let result = context.results[0];

  QuickSuggestTestUtils.assertTimestampsReplaced(result, {
    url: suggestion.click_url,
    sponsoredClickUrl: suggestion.click_url,
  });

  MerinoTestUtils.server.reset();
  merinoClient().resetSession();
});

// When both suggestion types are disabled but data collection is enabled, we
// should still send requests to Merino, and the requests should include an
// empty `providers` to tell Merino not to fetch any suggestions.
add_task(async function suggestedDisabled_dataCollectionEnabled() {
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", false);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", false);

  let context = createContext("test", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [],
  });

  // Check that the request is received and includes an empty `providers`.
  MerinoTestUtils.server.checkAndClearRequests([
    {
      params: {
        [MerinoTestUtils.SEARCH_PARAMS.QUERY]: "test",
        [MerinoTestUtils.SEARCH_PARAMS.SEQUENCE_NUMBER]: 0,
        [MerinoTestUtils.SEARCH_PARAMS.PROVIDERS]: "",
      },
    },
  ]);

  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);
  await QuickSuggestTestUtils.forceSync();
  merinoClient().resetSession();
});

// Test whether the blocking for Merino results works.
add_task(async function block() {
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  // Make sure the Merino suggestions have different URLs from the remote
  // settings suggestion.
  let { suggestions } = MerinoTestUtils.server.response.body;
  for (let i = 0; i < suggestions.length; i++) {
    let suggestion = suggestions[i];
    suggestion.url = "https://example.com/merino-url-" + i;
    await QuickSuggest.blockedSuggestions.add(suggestion.url);
  }

  const context = createContext(SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });

  await check_results({
    context,
    matches: [EXPECTED_REMOTE_SETTINGS_URLBAR_RESULT],
  });

  await QuickSuggest.blockedSuggestions.clear();
  MerinoTestUtils.server.reset();
  merinoClient().resetSession();
});

// Tests a Merino suggestion that is a top pick/best match.
add_task(async function bestMatch() {
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  // Set up a suggestion with `is_top_pick` and an unknown provider so that
  // UrlbarProviderQuickSuggest will make a default result for it.
  MerinoTestUtils.server.response.body.suggestions = [
    {
      is_top_pick: true,
      provider: "some_top_pick_provider",
      full_keyword: "full_keyword",
      title: "title",
      url: "url",
      icon: null,
      score: 1,
    },
  ];

  let context = createContext(SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      {
        isBestMatch: true,
        type: UrlbarUtils.RESULT_TYPE.URL,
        source: UrlbarUtils.RESULT_SOURCE.SEARCH,
        heuristic: false,
        payload: {
          telemetryType: "some_top_pick_provider",
          title: "title",
          url: "url",
          icon: null,
          qsSuggestion: "full_keyword",
          isSponsored: false,
          isBlockable: true,
          isManageable: true,
          displayUrl: "url",
          source: "merino",
          provider: "some_top_pick_provider",
        },
      },
    ],
  });

  // This isn't necessary since `check_results()` checks `isBestMatch`, but
  // check it here explicitly for good measure.
  Assert.ok(context.results[0].isBestMatch, "Result is a best match");

  MerinoTestUtils.server.reset();
  merinoClient().resetSession();
});

// Tests a sponsored suggestion that isn't managed by a feature.
add_task(async function unmanaged_sponsored() {
  await doUnmanagedTest({
    pref: "suggest.quicksuggest.sponsored",
    suggestion: {
      title: "Sponsored without feature",
      url: "https://example.com/sponsored-without-feature",
      provider: "sponsored-unrecognized-provider",
      is_sponsored: true,
    },
  });
});

// Tests a nonsponsored suggestion that isn't managed by a feature.
add_task(async function unmanaged_nonsponsored() {
  await doUnmanagedTest({
    pref: "suggest.quicksuggest.nonsponsored",
    suggestion: {
      title: "Nonsponsored without feature",
      url: "https://example.com/nonsponsored-without-feature",
      provider: "nonsponsored-unrecognized-provider",
      // no is_sponsored
    },
  });
});

async function doUnmanagedTest({ pref, suggestion }) {
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);
  await QuickSuggestTestUtils.forceSync();

  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);
  MerinoTestUtils.server.response.body.suggestions = [suggestion];

  let expectedResult = {
    type: UrlbarUtils.RESULT_TYPE.URL,
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    heuristic: false,
    payload: {
      title: suggestion.title,
      url: suggestion.url,
      displayUrl: suggestion.url.substring("https://".length),
      provider: suggestion.provider,
      telemetryType: suggestion.provider,
      isSponsored: !!suggestion.is_sponsored,
      source: "merino",
      isBlockable: true,
      isManageable: true,
      shouldShowUrl: true,
    },
  };

  // Do an initial search. Sponsored and nonsponsored suggestions are both
  // enabled, so the suggestion should be matched.
  info("Doing search 1");
  await check_results({
    context: createContext("test", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [expectedResult],
  });

  // Set the pref to false and do another search. The suggestion shouldn't be
  // matched.
  UrlbarPrefs.set(pref, false);
  await QuickSuggestTestUtils.forceSync();

  info("Doing search 2");
  await check_results({
    context: createContext("test", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [],
  });

  // Flip the pref back to true and do a third search.
  UrlbarPrefs.set(pref, true);
  await QuickSuggestTestUtils.forceSync();

  info("Doing search 3");
  let context = createContext("test", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [expectedResult],
  });

  // Trigger the dismiss command on the result.
  triggerCommand({
    feature: UrlbarProviderQuickSuggest,
    command: "dismiss",
    result: context.results[0],
    expectedCountsByCall: {
      removeResult: 1,
    },
  });
  await QuickSuggest.blockedSuggestions._test_readyPromise;

  Assert.ok(
    await QuickSuggest.blockedSuggestions.isResultBlocked(context.results[0]),
    "The suggestion URL should be blocked"
  );

  await QuickSuggest.blockedSuggestions.clear();
  MerinoTestUtils.server.reset();
  merinoClient().resetSession();
}

function merinoClient() {
  return QuickSuggest.getFeature("SuggestBackendMerino")?.client;
}

async function resetRemoteSettingsData() {
  await QuickSuggestTestUtils.setRemoteSettingsRecords([
    {
      collection: QuickSuggestTestUtils.RS_COLLECTION.AMP,
      type: QuickSuggestTestUtils.RS_TYPE.AMP,
      attachment: REMOTE_SETTINGS_RESULTS,
    },
  ]);
}
