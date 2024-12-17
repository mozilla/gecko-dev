/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the quick suggest weather feature.
//
// w/r/t weather queries with cities, note that the Suggest Rust component
// handles city/region parsing and has extensive tests for that. Here we need to
// test our geolocation logic, make sure Merino is called with the correct
// city/region, and make sure the urlbar result has the correct city.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  MerinoClient: "resource:///modules/MerinoClient.sys.mjs",
  UrlbarProviderPlaces: "resource:///modules/UrlbarProviderPlaces.sys.mjs",
});

const HISTOGRAM_LATENCY = "FX_URLBAR_MERINO_LATENCY_WEATHER_MS";
const HISTOGRAM_RESPONSE = "FX_URLBAR_MERINO_RESPONSE_WEATHER";

const { WEATHER_SUGGESTION } = MerinoTestUtils;

let gWeather;

add_setup(async () => {
  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    prefs: [
      ["suggest.quicksuggest.sponsored", true],
      ["weather.featureGate", true],
    ],
    remoteSettingsRecords: [
      QuickSuggestTestUtils.weatherRecord(),
      QuickSuggestTestUtils.geonamesRecord(),
    ],
  });

  await MerinoTestUtils.initWeather();

  gWeather = QuickSuggest.getFeature("WeatherSuggestions");
});

// The feature should be properly enabled according to `weather.featureGate`.
add_task(async function disableAndEnable_featureGate() {
  await doBasicDisableAndEnableTest("weather.featureGate");
});

// The feature should be properly enabled according to `suggest.weather`.
add_task(async function disableAndEnable_suggestPref() {
  await doBasicDisableAndEnableTest("suggest.weather");
});

// The feature should be properly enabled according to
// `suggest.quicksuggest.sponsored`.
add_task(async function disableAndEnable_sponsoredPref() {
  await doBasicDisableAndEnableTest("suggest.quicksuggest.sponsored");
});

async function doBasicDisableAndEnableTest(pref) {
  // Disable the feature. It should be immediately uninitialized.
  UrlbarPrefs.set(pref, false);
  assertDisabled({
    message: "After disabling",
  });

  // No suggestion should be returned for a search.
  let context = createContext("weather", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [],
  });

  let histograms = MerinoTestUtils.getAndClearHistograms({
    extraLatency: HISTOGRAM_LATENCY,
    extraResponse: HISTOGRAM_RESPONSE,
  });

  // Re-enable the feature.
  info("Re-enable the feature");
  UrlbarPrefs.set(pref, true);

  // The suggestion should be returned for a search.
  context = createContext("weather", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [QuickSuggestTestUtils.weatherResult()],
  });

  MerinoTestUtils.checkAndClearHistograms({
    histograms,
    response: "success",
    latencyRecorded: true,
    client: gWeather._test_merino,
  });
}

// Tests a Merino fetch that doesn't return a suggestion.
add_task(async function noSuggestion() {
  let histograms = MerinoTestUtils.getAndClearHistograms({
    extraLatency: HISTOGRAM_LATENCY,
    extraResponse: HISTOGRAM_RESPONSE,
  });

  let { suggestions } = MerinoTestUtils.server.response.body;
  MerinoTestUtils.server.response.body.suggestions = [];

  let context = createContext("weather", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [],
  });

  MerinoTestUtils.checkAndClearHistograms({
    histograms,
    response: "no_suggestion",
    latencyRecorded: true,
    client: gWeather._test_merino,
  });

  MerinoTestUtils.server.response.body.suggestions = suggestions;
});

// When the query matches both the weather suggestion and a previous visit to
// the suggestion's URL, the suggestion should be shown and the history visit
// should not be shown.
add_task(async function urlAlreadyInHistory() {
  // A visit to the weather suggestion's exact URL.
  let suggestionVisit = {
    uri: MerinoTestUtils.WEATHER_SUGGESTION.url,
    title: MerinoTestUtils.WEATHER_SUGGESTION.title,
  };

  // A visit to a totally unrelated URL that also matches "weather" just to make
  // sure the Places provider is enabled and returning matches as expected.
  let otherVisit = {
    uri: "https://example.com/some-other-weather-page",
    title: "Some other weather page",
  };

  await PlacesTestUtils.addVisits([suggestionVisit, otherVisit]);

  // First make sure both visit results are matched by doing a search with only
  // the Places provider.
  info("Doing first search");
  let context = createContext("weather", {
    providers: [UrlbarProviderPlaces.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, otherVisit),
      makeVisitResult(context, suggestionVisit),
    ],
  });

  // Now do a search with both the Suggest and Places providers.
  info("Doing second search");
  context = createContext("weather", {
    providers: [UrlbarProviderQuickSuggest.name, UrlbarProviderPlaces.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      // The visit result to the unrelated URL will be first since the weather
      // suggestion has a `suggestedIndex` of 1.
      makeVisitResult(context, otherVisit),
      QuickSuggestTestUtils.weatherResult(),
    ],
  });

  await PlacesUtils.history.clear();
});

// Tests a Merino fetch that fails with a network error.
add_task(async function networkError() {
  // This task is unreliable on Windows. See the comment in
  // `MerinoTestUtils.withNetworkError()`.
  if (AppConstants.platform == "win") {
    Assert.ok(true, "Skipping this task on Windows");
    return;
  }

  let histograms = MerinoTestUtils.getAndClearHistograms({
    extraLatency: HISTOGRAM_LATENCY,
    extraResponse: HISTOGRAM_RESPONSE,
  });

  await MerinoTestUtils.server.withNetworkError(async () => {
    let context = createContext("weather", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    });
    await check_results({
      context,
      matches: [],
    });
  });

  MerinoTestUtils.checkAndClearHistograms({
    histograms,
    response: "network_error",
    latencyRecorded: false,
    client: gWeather._test_merino,
  });
});

// Tests a Merino fetch that fails with an HTTP error.
add_task(async function httpError() {
  let histograms = MerinoTestUtils.getAndClearHistograms({
    extraLatency: HISTOGRAM_LATENCY,
    extraResponse: HISTOGRAM_RESPONSE,
  });

  MerinoTestUtils.server.response = { status: 500 };

  let context = createContext("weather", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [],
  });

  MerinoTestUtils.checkAndClearHistograms({
    histograms,
    response: "http_error",
    latencyRecorded: true,
    client: gWeather._test_merino,
  });

  MerinoTestUtils.server.reset();
  MerinoTestUtils.server.response.body.suggestions = [WEATHER_SUGGESTION];
});

// Tests a Merino fetch that fails due to a client timeout.
add_task(async function clientTimeout() {
  let histograms = MerinoTestUtils.getAndClearHistograms({
    extraLatency: HISTOGRAM_LATENCY,
    extraResponse: HISTOGRAM_RESPONSE,
  });

  // Make the server return a delayed response so the Merino client times out
  // waiting for it.
  MerinoTestUtils.server.response.delay = 400;

  // Make the client time out immediately.
  gWeather._test_setTimeoutMs(1);

  // Set up a promise that will be resolved when the client finally receives the
  // response.
  let responsePromise = gWeather._test_merino.waitForNextResponse();

  let context = createContext("weather", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [],
  });

  MerinoTestUtils.checkAndClearHistograms({
    histograms,
    response: "timeout",
    latencyRecorded: false,
    latencyStopwatchRunning: true,
    client: gWeather._test_merino,
  });

  // Await the response.
  await responsePromise;

  // The `checkAndClearHistograms()` call above cleared the histograms. After
  // that, nothing else should have been recorded for the response.
  MerinoTestUtils.checkAndClearHistograms({
    histograms,
    response: null,
    latencyRecorded: true,
    client: gWeather._test_merino,
  });

  gWeather._test_setTimeoutMs(-1);
  delete MerinoTestUtils.server.response.delay;
});

// Locale task for when this test runs on an en-US OS.
add_task(async function locale_enUS() {
  await doLocaleTest({
    shouldRunTask: osLocale => osLocale == "en-US",
    osUnit: "f",
    unitsByLocale: {
      "en-US": "f",
      // When the app's locale is set to any en-* locale, F will be used because
      // `regionalPrefsLocales` will prefer the en-US OS locale.
      "en-CA": "f",
      "en-GB": "f",
      de: "c",
    },
  });
});

// Locale task for when this test runs on a non-US English OS.
add_task(async function locale_nonUSEnglish() {
  await doLocaleTest({
    shouldRunTask: osLocale => osLocale.startsWith("en") && osLocale != "en-US",
    osUnit: "c",
    unitsByLocale: {
      // When the app's locale is set to en-US, C will be used because
      // `regionalPrefsLocales` will prefer the non-US English OS locale.
      "en-US": "c",
      "en-CA": "c",
      "en-GB": "c",
      de: "c",
    },
  });
});

// Locale task for when this test runs on a non-English OS.
add_task(async function locale_nonEnglish() {
  await doLocaleTest({
    shouldRunTask: osLocale => !osLocale.startsWith("en"),
    osUnit: "c",
    unitsByLocale: {
      "en-US": "f",
      "en-CA": "c",
      "en-GB": "c",
      de: "c",
    },
  });
});

/**
 * Testing locales is tricky due to the weather feature's use of
 * `Services.locale.regionalPrefsLocales`. By default `regionalPrefsLocales`
 * prefers the OS locale if its language is the same as the app locale's
 * language; otherwise it prefers the app locale. For example, assuming the OS
 * locale is en-CA, then if the app locale is en-US it will prefer en-CA since
 * both are English, but if the app locale is de it will prefer de. If the pref
 * `intl.regional_prefs.use_os_locales` is set, then the OS locale is always
 * preferred.
 *
 * This function tests a given set of locales with and without
 * `intl.regional_prefs.use_os_locales` set.
 *
 * @param {object} options
 *   Options
 * @param {Function} options.shouldRunTask
 *   Called with the OS locale. Should return true if the function should run.
 *   Use this to skip tasks that don't target a desired OS locale.
 * @param {string} options.osUnit
 *   The expected "c" or "f" unit for the OS locale.
 * @param {object} options.unitsByLocale
 *   The expected "c" or "f" unit when the app's locale is set to particular
 *   locales. This should be an object that maps locales to expected units. For
 *   each locale in the object, the app's locale is set to that locale and the
 *   actual unit is expected to be the unit in the object.
 */
async function doLocaleTest({ shouldRunTask, osUnit, unitsByLocale }) {
  Services.prefs.setBoolPref("intl.regional_prefs.use_os_locales", true);
  let osLocale = Services.locale.regionalPrefsLocales[0];
  Services.prefs.clearUserPref("intl.regional_prefs.use_os_locales");

  if (!shouldRunTask(osLocale)) {
    info("Skipping task, should not run for this OS locale");
    return;
  }

  // Sanity check initial locale info.
  Assert.equal(
    Services.locale.appLocaleAsBCP47,
    "en-US",
    "Initial app locale should be en-US"
  );
  Assert.ok(
    !Services.prefs.getBoolPref("intl.regional_prefs.use_os_locales"),
    "intl.regional_prefs.use_os_locales should be false initially"
  );

  // Check locales.
  for (let [locale, temperatureUnit] of Object.entries(unitsByLocale)) {
    await QuickSuggestTestUtils.withLocales({
      locales: [locale],
      callback: async () => {
        info("Checking locale: " + locale);
        await check_results({
          context: createContext("weather", {
            providers: [UrlbarProviderQuickSuggest.name],
            isPrivate: false,
          }),
          matches: [QuickSuggestTestUtils.weatherResult({ temperatureUnit })],
        });

        info(
          "Checking locale with intl.regional_prefs.use_os_locales: " + locale
        );
        Services.prefs.setBoolPref("intl.regional_prefs.use_os_locales", true);
        await check_results({
          context: createContext("weather", {
            providers: [UrlbarProviderQuickSuggest.name],
            isPrivate: false,
          }),
          matches: [
            QuickSuggestTestUtils.weatherResult({ temperatureUnit: osUnit }),
          ],
        });
        Services.prefs.clearUserPref("intl.regional_prefs.use_os_locales");
      },
    });
  }
}

// Blocks a result and makes sure the weather pref is disabled.
add_task(async function block() {
  // Sanity check initial state.
  Assert.ok(
    UrlbarPrefs.get("suggest.weather"),
    "Sanity check: suggest.weather is true initially"
  );

  // Do a search so we can get an actual result.
  let context = createContext("weather", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [QuickSuggestTestUtils.weatherResult()],
  });

  // Block the result.
  const controller = UrlbarTestUtils.newMockController();
  controller.setView({
    get visibleResults() {
      return context.results;
    },
    controller: {
      removeResult() {},
    },
  });
  let result = context.results[0];
  let provider = UrlbarProvidersManager.getProvider(result.providerName);
  Assert.ok(provider, "Sanity check: Result provider found");

  provider.onEngagement(context, controller, {
    result,
    selType: "dismiss",
    selIndex: context.results[0].rowIndex,
  });
  Assert.ok(
    !UrlbarPrefs.get("suggest.weather"),
    "suggest.weather is false after blocking the result"
  );

  // Do a second search. Nothing should be returned.
  context = createContext("weather", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [],
  });

  // Re-enable the pref and (when Rust is disabled) wait for keywords to be
  // re-synced from remote settings.
  UrlbarPrefs.set("suggest.weather", true);
  await QuickSuggestTestUtils.forceSync();
});

// When a Nimbus experiment is installed, it should override the remote settings
// weather record.
add_task(async function nimbusOverride() {
  let defaultResult = QuickSuggestTestUtils.weatherResult();

  // Verify a search works as expected with the default remote settings weather
  // record (which was added in the init task).
  await check_results({
    context: createContext("weather", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [defaultResult],
  });

  // Install an experiment with a different min keyword length.
  let nimbusCleanup = await UrlbarTestUtils.initNimbusFeature({
    weatherKeywordsMinimumLength: 999,
  });

  // The suggestion shouldn't be returned anymore.
  await check_results({
    context: createContext("weather", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [],
  });

  // Uninstall the experiment.
  await nimbusCleanup();

  // The suggestion should be returned again.
  await check_results({
    context: createContext("weather", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [defaultResult],
  });
});

// Tests queries that include a city without a region and where Merino does not
// return a geolocation.
add_task(async function cityQueries_noGeo() {
  await doCityTest({
    desc: "Should match most populous Waterloo (Waterloo IA)",
    query: "waterloo",
    geolocation: null,
    expected: {
      geolocationCalled: true,
      weatherParams: {
        city: "Waterloo",
        region: "IA",
        country: "US",
      },
      suggestionCity: "Waterloo",
    },
  });
});

// Tests queries that include a city without a region and where Merino returns a
// geolocation with geographic coordinates.
add_task(async function cityQueries_geoCoords() {
  await doCityTest({
    desc: "Coordinates closer to Waterloo IA, so should match it",
    query: "waterloo",
    geolocation: {
      location: {
        latitude: 41.0,
        longitude: -93.0,
      },
    },
    expected: {
      geolocationCalled: true,
      weatherParams: {
        city: "Waterloo",
        region: "IA",
        country: "US",
      },
      suggestionCity: "Waterloo",
    },
  });

  await doCityTest({
    desc: "Coordinates closer to Waterloo AL, so should match it",
    query: "waterloo",
    geolocation: {
      location: {
        latitude: 33.0,
        longitude: -87.0,
      },
    },
    expected: {
      geolocationCalled: true,
      weatherParams: {
        city: "Waterloo",
        region: "AL",
        country: "US",
      },
      suggestionCity: "Waterloo",
    },
  });

  // This assumes the mock GeoNames data includes "Twin City A" and
  // "Twin City B" and they're <= 5 km apart.
  await doCityTest({
    desc: "When multiple cities are tied for nearest (within the accuracy radius), the most populous one should match",
    query: "weather twin city",
    geolocation: {
      location: {
        latitude: 0.0,
        longitude: 0.0,
        // 5 km radius
        accuracy: 5,
      },
    },
    expected: {
      geolocationCalled: true,
      weatherParams: {
        city: "Twin City B",
        region: "GA",
        country: "US",
      },
      suggestionCity: "Twin City B",
    },
  });
});

// Tests queries that include a city without a region and where Merino returns a
// geolocation with only region and country codes, no geographic coordinates.
add_task(async function cityQueries_geoRegion() {
  await doCityTest({
    desc: "Should match Waterloo IA",
    query: "waterloo",
    geolocation: {
      region_code: "IA",
      country_code: "US",
    },
    expected: {
      geolocationCalled: true,
      weatherParams: {
        city: "Waterloo",
        region: "IA",
        country: "US",
      },
      suggestionCity: "Waterloo",
    },
  });

  await doCityTest({
    desc: "Should match Waterloo AL",
    query: "waterloo",
    geolocation: {
      region_code: "AL",
      country_code: "US",
    },
    expected: {
      geolocationCalled: true,
      weatherParams: {
        city: "Waterloo",
        region: "AL",
        country: "US",
      },
      suggestionCity: "Waterloo",
    },
  });

  await doCityTest({
    desc: "Rust did not return Waterloo NY, so should match most populous Waterloo (Waterloo IA)",
    query: "waterloo",
    geolocation: {
      region_code: "NY",
      country_code: "US",
    },
    expected: {
      geolocationCalled: true,
      weatherParams: {
        city: "Waterloo",
        region: "IA",
        country: "US",
      },
      suggestionCity: "Waterloo",
    },
  });

  await doCityTest({
    desc: "Rust did not return Waterloo ON CA, so should match most populous Waterloo (Waterloo IA)",
    query: "waterloo",
    geolocation: {
      region_code: "08",
      country_code: "CA",
    },
    expected: {
      geolocationCalled: true,
      weatherParams: {
        city: "Waterloo",
        region: "IA",
        country: "US",
      },
      suggestionCity: "Waterloo",
    },
  });

  await doCityTest({
    desc: "Query matches a US and CA city, geolocation is US, so should match US city",
    query: "us ca city",
    geolocation: {
      region_code: "HI",
      country_code: "US",
    },
    expected: {
      geolocationCalled: true,
      weatherParams: {
        city: "US CA City",
        region: "IA",
        country: "US",
      },
      suggestionCity: "US CA City",
    },
  });

  await doCityTest({
    desc: "Query matches a US and CA city, geolocation is CA, so should match CA city",
    query: "us ca city",
    geolocation: {
      region_code: "01",
      country_code: "CA",
    },
    expected: {
      geolocationCalled: true,
      weatherParams: {
        city: "US CA City",
        region: "08",
        country: "CA",
      },
      suggestionCity: "US CA City",
    },
  });
});

// Tests queries that include both a city and a region.
add_task(async function cityRegionQueries() {
  await doCityTest({
    desc: "Waterloo IA directly queried",
    query: "waterloo ia",
    geolocation: null,
    expected: {
      geolocationCalled: false,
      weatherParams: {
        city: "Waterloo",
        region: "IA",
        country: "US",
      },
      suggestionCity: "Waterloo",
    },
  });

  await doCityTest({
    desc: "Waterloo AL directly queried",
    query: "waterloo al",
    geolocation: null,
    expected: {
      geolocationCalled: false,
      weatherParams: {
        city: "Waterloo",
        region: "AL",
        country: "US",
      },
      suggestionCity: "Waterloo",
    },
  });

  await doCityTest({
    desc: "Waterloo NY directly queried, but Rust didn't return Waterloo NY, so no match",
    query: "waterloo ny",
    geolocation: null,
    expected: null,
  });
});

// Tests weather queries that don't include a city.
add_task(async function noCityQuery() {
  await doCityTest({
    desc: "No city in query, so only one call to Merino should be made and Merino does the geolocation internally",
    query: "weather",
    geolocation: null,
    expected: {
      geolocationCalled: false,
      weatherParams: {},
      suggestionCity: WEATHER_SUGGESTION.city_name,
    },
  });
});

async function doCityTest({ desc, query, geolocation, expected }) {
  info("Doing city test: " + JSON.stringify({ desc, query }));

  if (expected) {
    expected.weatherParams.q ??= "";
  }

  let callsByProvider = await doSearch({
    query,
    geolocation,
    suggestionCity: expected?.suggestionCity,
  });

  // Check the Merino calls.
  Assert.equal(
    callsByProvider.geolocation?.length || 0,
    expected?.geolocationCalled ? 1 : 0,
    "geolocation provider should have been called the correct number of times"
  );
  Assert.equal(
    callsByProvider.accuweather?.length || 0,
    expected ? 1 : 0,
    "accuweather provider should have been called the correct number of times"
  );
  if (expected) {
    for (let [key, value] of Object.entries(expected.weatherParams)) {
      Assert.strictEqual(
        callsByProvider.accuweather[0].get(key),
        value,
        "Weather param should be correct: " + key
      );
    }
  }
}

// `MerinoClient` should cache Merino responses for geolocation and weather.
add_task(async function merinoCache() {
  let query = "waterloo";
  let geolocation = {
    location: {
      latitude: 41.0,
      longitude: -93.0,
    },
  };

  MerinoTestUtils.enableClientCache(true);

  let startDateMs = Date.now();
  let sandbox = sinon.createSandbox();
  let dateNowStub = sandbox.stub(
    Cu.getGlobalForObject(MerinoClient).Date,
    "now"
  );
  dateNowStub.returns(startDateMs);

  // Search 1: Firefox should call Merino for both geolocation and weather and
  // cache the responses.
  info("Doing search 1");
  let callsByProvider = await doSearch({
    query,
    geolocation,
    suggestionCity: "Waterloo",
  });
  info("search 1 callsByProvider: " + JSON.stringify(callsByProvider));
  Assert.equal(
    callsByProvider.geolocation.length,
    1,
    "geolocation provider should have been called on search 1"
  );
  Assert.equal(
    callsByProvider.accuweather.length,
    1,
    "accuweather provider should have been called on search 1"
  );

  // Set the date forward 0.5 minutes, which is shorter than the geolocation
  // cache period of 2 minutes and the weather cache period of 1 minute.
  dateNowStub.returns(startDateMs + 0.5 * 60 * 1000);

  // Search 2: Firefox should use the cached responses, so it should not call
  // Merino.
  info("Doing search 2");
  callsByProvider = await doSearch({
    query,
    suggestionCity: "Waterloo",
  });
  info("search 2 callsByProvider: " + JSON.stringify(callsByProvider));
  Assert.ok(
    !callsByProvider.geolocation,
    "geolocation provider should not have been called on search 2"
  );
  Assert.ok(
    !callsByProvider.accuweather,
    "accuweather provider should not have been called on search 2"
  );

  // Set the date forward 1.5 minutes, which is shorter than the geolocation
  // cache period but longer than the weather cache period.
  dateNowStub.returns(startDateMs + 1.5 * 60 * 1000);

  // Search 3: Firefox should call Merino for the weather suggestion but not for
  // geolocation.
  info("Doing search 3");
  callsByProvider = await doSearch({
    query,
    suggestionCity: "Waterloo",
  });
  info("search 3 callsByProvider: " + JSON.stringify(callsByProvider));
  Assert.ok(
    !callsByProvider.geolocation,
    "geolocation provider should not have been called on search 3"
  );
  Assert.equal(
    callsByProvider.accuweather.length,
    1,
    "accuweather provider should have been called on search 3"
  );

  // Set the date forward 3 minutes.
  dateNowStub.returns(startDateMs + 3 * 60 * 1000);

  // Search 4: Firefox should call Merino for both weather and geolocation.
  info("Doing search 4");
  callsByProvider = await doSearch({
    query,
    suggestionCity: "Waterloo",
  });
  info("search 4 callsByProvider: " + JSON.stringify(callsByProvider));
  Assert.equal(
    callsByProvider.geolocation.length,
    1,
    "geolocation provider should have been called on search 4"
  );
  Assert.equal(
    callsByProvider.accuweather.length,
    1,
    "accuweather provider should have been called on search 4"
  );

  sandbox.restore();
  MerinoTestUtils.enableClientCache(false);
});

async function doSearch({ query, geolocation, suggestionCity }) {
  let callsByProvider = {};

  // Set up the Merino request handler.
  MerinoTestUtils.server.requestHandler = req => {
    let params = new URLSearchParams(req.queryString);
    let provider = params.get("providers");
    callsByProvider[provider] ||= [];
    callsByProvider[provider].push(params);

    // Handle geolocation requests.
    if (provider == "geolocation") {
      return {
        body: {
          request_id: "request_id",
          suggestions: !geolocation
            ? []
            : [
                {
                  custom_details: { geolocation },
                },
              ],
        },
      };
    }

    // Handle accuweather requests.
    Assert.equal(
      provider,
      "accuweather",
      "Sanity check: If the request isn't geolocation, it should be accuweather"
    );
    let suggestion = { ...WEATHER_SUGGESTION };
    if (suggestionCity) {
      suggestion = {
        ...suggestion,
        title: "Weather for " + suggestionCity,
        city_name: suggestionCity,
      };
    }
    return {
      body: {
        request_id: "request_id",
        suggestions: [suggestion],
      },
    };
  };

  // Do a search.
  await check_results({
    context: createContext(query, {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: !suggestionCity
      ? []
      : [
          QuickSuggestTestUtils.weatherResult({
            city: suggestionCity,
          }),
        ],
  });

  MerinoTestUtils.server.requestHandler = null;
  return callsByProvider;
}

function assertDisabled({ message }) {
  info("Asserting feature is disabled");
  if (message) {
    info(message);
  }
  Assert.strictEqual(gWeather._test_merino, null, "Merino client is null");
}
