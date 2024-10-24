/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests for Yelp suggestions served by the Suggest ML backend.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  MLSuggest: "resource:///modules/urlbar/private/MLSuggest.sys.mjs",
});

const REMOTE_SETTINGS_RECORDS = [
  {
    type: "yelp-suggestions",
    attachment: {
      // The "coffee" subject is important: It's how `YelpSuggestions` looks up
      // the Yelp icon and score right now.
      subjects: ["coffee"],
      preModifiers: [],
      postModifiers: [],
      locationSigns: [
        { keyword: "in", needLocation: true },
        { keyword: "nearby", needLocation: false },
      ],
      yelpModifiers: [],
      icon: "1234",
      score: 0.5,
    },
  },
];

let gSandbox;
let gMakeSuggestionsStub;

add_setup(async function init() {
  // Stub `MLSuggest`.
  gSandbox = sinon.createSandbox();
  gSandbox.stub(MLSuggest, "initialize");
  gSandbox.stub(MLSuggest, "shutdown");
  gMakeSuggestionsStub = gSandbox.stub(MLSuggest, "makeSuggestions");

  // Set up Rust Yelp suggestions that can be matched on the keyword "coffee".
  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    prefs: [
      ["browser.ml.enable", true],
      ["quicksuggest.mlEnabled", true],
      ["suggest.quicksuggest.nonsponsored", true],
      ["suggest.quicksuggest.sponsored", true],
      ["yelp.mlEnabled", true],
    ],
    remoteSettingsRecords: REMOTE_SETTINGS_RECORDS,
  });

  await MerinoTestUtils.initGeolocation();
});

// Runs through a variety of mock intents.
add_task(async function intents() {
  let tests = [
    {
      desc: "subject with no location",
      ml: { intent: "yelp_intent", subject: "burgers" },
      expected: {
        url: "https://www.yelp.com/search?find_desc=burgers&find_loc=Yokohama%2C+Kanagawa",
        originalUrl: "https://www.yelp.com/search?find_desc=burgers",
        displayUrl:
          "yelp.com/search?find_desc=burgers&find_loc=Yokohama,+Kanagawa",
        title: "burgers in Yokohama, Kanagawa",
      },
    },
    {
      desc: "subject with null city and null state",
      ml: {
        intent: "yelp_intent",
        subject: "burgers",
        location: { city: null, state: null },
      },
      expected: {
        url: "https://www.yelp.com/search?find_desc=burgers&find_loc=Yokohama%2C+Kanagawa",
        originalUrl: "https://www.yelp.com/search?find_desc=burgers",
        displayUrl:
          "yelp.com/search?find_desc=burgers&find_loc=Yokohama,+Kanagawa",
        title: "burgers in Yokohama, Kanagawa",
      },
    },
    {
      desc: "subject with city",
      ml: {
        intent: "yelp_intent",
        subject: "burgers",
        location: { city: "Waterloo", state: null },
      },
      expected: {
        url: "https://www.yelp.com/search?find_desc=burgers&find_loc=Waterloo",
        originalUrl:
          "https://www.yelp.com/search?find_desc=burgers&find_loc=Waterloo",
        displayUrl: "yelp.com/search?find_desc=burgers&find_loc=Waterloo",
        title: "burgers in Waterloo",
      },
    },
    {
      desc: "subject with state",
      ml: {
        intent: "yelp_intent",
        subject: "burgers",
        location: { city: null, state: "IA" },
      },
      expected: {
        url: "https://www.yelp.com/search?find_desc=burgers&find_loc=IA",
        originalUrl:
          "https://www.yelp.com/search?find_desc=burgers&find_loc=IA",
        displayUrl: "yelp.com/search?find_desc=burgers&find_loc=IA",
        title: "burgers in IA",
      },
    },
    {
      desc: "subject with city and state",
      ml: {
        intent: "yelp_intent",
        subject: "burgers",
        location: { city: "Waterloo", state: "IA" },
      },
      expected: {
        url: "https://www.yelp.com/search?find_desc=burgers&find_loc=Waterloo%2C+IA",
        originalUrl:
          "https://www.yelp.com/search?find_desc=burgers&find_loc=Waterloo%2C+IA",
        displayUrl: "yelp.com/search?find_desc=burgers&find_loc=Waterloo,+IA",
        title: "burgers in Waterloo, IA",
      },
    },
    {
      desc: "no subject with no location",
      ml: {
        intent: "yelp_intent",
        subject: "",
      },
      expected: null,
    },
    {
      desc: "no subject with null city and null state",
      ml: {
        intent: "yelp_intent",
        subject: "",
        location: { city: null, state: null },
      },
      expected: null,
    },
    {
      desc: "no subject with city",
      ml: {
        intent: "yelp_intent",
        subject: "",
        location: { city: "Waterloo", state: null },
      },
      expected: {
        url: "https://www.yelp.com/search?find_loc=Waterloo",
        originalUrl: "https://www.yelp.com/search?find_loc=Waterloo",
        displayUrl: "yelp.com/search?find_loc=Waterloo",
        title: "Waterloo",
      },
    },
    {
      desc: "no subject with state",
      ml: {
        intent: "yelp_intent",
        subject: "",
        location: { city: null, state: "IA" },
      },
      expected: {
        url: "https://www.yelp.com/search?find_loc=IA",
        originalUrl: "https://www.yelp.com/search?find_loc=IA",
        displayUrl: "yelp.com/search?find_loc=IA",
        title: "IA",
      },
    },
    {
      desc: "no subject with city and state",
      ml: {
        intent: "yelp_intent",
        subject: "",
        location: { city: "Waterloo", state: "IA" },
      },
      expected: {
        url: "https://www.yelp.com/search?find_loc=Waterloo%2C+IA",
        originalUrl: "https://www.yelp.com/search?find_loc=Waterloo%2C+IA",
        displayUrl: "yelp.com/search?find_loc=Waterloo,+IA",
        title: "Waterloo, IA",
      },
    },
    {
      desc: "unrecognized intent",
      ml: { intent: "unrecognized_intent" },
      expected: null,
    },
    {
      desc: "only Rust returns a suggestion",
      query: "coffee",
      ml: null,
      expected: {
        source: "rust",
        provider: "Yelp",
        url: "https://www.yelp.com/search?find_desc=coffee&find_loc=Yokohama%2C+Kanagawa",
        originalUrl: "https://www.yelp.com/search?find_desc=coffee",
        displayUrl:
          "yelp.com/search?find_desc=coffee&find_loc=Yokohama,+Kanagawa",
        title: "coffee in Yokohama, Kanagawa",
      },
    },
    {
      desc: "both ML and Rust return a suggestion",
      query: "coffee",
      ml: {
        intent: "yelp_intent",
        subject: "coffee",
        location: { city: "Waterloo", state: null },
      },
      expected: {
        url: "https://www.yelp.com/search?find_desc=coffee&find_loc=Waterloo",
        originalUrl:
          "https://www.yelp.com/search?find_desc=coffee&find_loc=Waterloo",
        displayUrl: "yelp.com/search?find_desc=coffee&find_loc=Waterloo",
        title: "coffee in Waterloo",
      },
    },
  ];

  for (let { desc, ml, expected, query = "test" } of tests) {
    info("Doing subtest: " + JSON.stringify({ desc, query, ml }));

    // Do a query with ML enabled. If the expected result is from Rust, the
    // query shouldn't return any results because *only* ML results are returned
    // when ML is enabled.
    gMakeSuggestionsStub.returns(ml);
    await check_results({
      context: createContext(query, {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
      matches:
        expected && expected.source != "rust"
          ? [makeExpectedResult(expected)]
          : [],
    });

    // If the expected result is from Rust, disable ML and query again to make
    // sure it matches.
    if (expected?.source == "rust") {
      UrlbarPrefs.set("yelp.mlEnabled", false);
      await check_results({
        context: createContext(query, {
          providers: [UrlbarProviderQuickSuggest.name],
          isPrivate: false,
        }),
        matches: [makeExpectedResult(expected)],
      });
      UrlbarPrefs.set("yelp.mlEnabled", true);
    }
  }
});

// The metadata cache should be populated from the "coffee" Rust suggestion when
// it's present in remote settings.
add_task(async function cache_fromRust() {
  await doCacheTest({
    expectedScore: REMOTE_SETTINGS_RECORDS[0].attachment.score,
    expectedRust: {
      source: "rust",
      provider: "Yelp",
      url: "https://www.yelp.com/search?find_desc=coffee&find_loc=Yokohama%2C+Kanagawa",
      originalUrl: "https://www.yelp.com/search?find_desc=coffee",
      displayUrl:
        "yelp.com/search?find_desc=coffee&find_loc=Yokohama,+Kanagawa",
      title: "coffee in Yokohama, Kanagawa",
    },
  });
});

// The metadata cache should be populated with default values when the "coffee"
// Rust suggestion is not present in remote settings.
add_task(async function cache_defaultValues() {
  await QuickSuggestTestUtils.setRemoteSettingsRecords([]);
  await doCacheTest({
    // This value is hardcoded in `YelpSuggestions` as the default.
    expectedScore: 0.25,
    expectedRust: null,
  });
  await QuickSuggestTestUtils.setRemoteSettingsRecords(REMOTE_SETTINGS_RECORDS);
});

async function doCacheTest({ expectedScore, expectedRust }) {
  // Do a search with ML disabled to verify the Rust suggestion is matched as
  // expected.
  info("Doing search with ML disabled");
  UrlbarPrefs.set("yelp.mlEnabled", false);
  await check_results({
    context: createContext("coffee", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: expectedRust ? [makeExpectedResult(expectedRust)] : [],
  });
  UrlbarPrefs.set("yelp.mlEnabled", true);

  gMakeSuggestionsStub.returns({
    intent: "yelp_intent",
    subject: "burgers",
    location: { city: "Waterloo", state: null },
  });

  // Stub `YelpSuggestions.makeResult()` so we can get the suggestion object
  // passed into it.
  let passedSuggestion;
  let feature = QuickSuggest.getFeature("YelpSuggestions");
  let stub = gSandbox
    .stub(feature, "makeResult")
    .callsFake((queryContext, suggestion, searchString) => {
      passedSuggestion = suggestion;
      return stub.wrappedMethod.call(
        feature,
        queryContext,
        suggestion,
        searchString
      );
    });

  // Do a search with ML enabled.
  info("Doing search with ML enabled");
  feature._test_invalidateMetadataCache();
  await check_results({
    context: createContext("test", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [
      makeExpectedResult({
        url: "https://www.yelp.com/search?find_desc=burgers&find_loc=Waterloo",
        originalUrl:
          "https://www.yelp.com/search?find_desc=burgers&find_loc=Waterloo",
        displayUrl: "yelp.com/search?find_desc=burgers&find_loc=Waterloo",
        title: "burgers in Waterloo",
      }),
    ],
  });

  stub.restore();

  // The score of the ML suggestion passed into `makeResult()` should have been
  // taken from the metadata cache.
  Assert.ok(
    passedSuggestion,
    "makeResult should have been called and passed the suggestion"
  );
  Assert.equal(
    passedSuggestion.score,
    expectedScore,
    "The suggestion should have borrowed its score from the Rust 'coffee' suggestion"
  );
}

function makeExpectedResult({
  url,
  title,
  source = "ml",
  provider = "yelp_intent",
  originalUrl = undefined,
  displayUrl = undefined,
}) {
  const utmParameters = "&utm_medium=partner&utm_source=mozilla";

  originalUrl ??= url;

  displayUrl =
    (displayUrl ??
      url
        .replace(/^https:\/\/www[.]/, "")
        .replace("%20", " ")
        .replace("%2C", ",")) + utmParameters;

  url += utmParameters;

  return {
    type: UrlbarUtils.RESULT_TYPE.URL,
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    heuristic: false,
    payload: {
      source,
      provider,
      telemetryType: "yelp",
      bottomTextL10n: { id: "firefox-suggest-yelp-bottom-text" },
      url,
      originalUrl,
      title,
      displayUrl,
      icon: null,
    },
  };
}
