/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the keywords behavior of quick suggest weather.

"use strict";

add_setup(async () => {
  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    remoteSettingsRecords: [QuickSuggestTestUtils.weatherRecord()],
    prefs: [
      ["suggest.quicksuggest.sponsored", true],
      ["weather.featureGate", true],
    ],
  });
  await MerinoTestUtils.initWeather();
});

// * Settings data: none
// * Nimbus values: none
// * Min keyword length pref: none
// * Expected: no suggestion
add_task(async function () {
  await doKeywordsTest({
    desc: "No data",
    tests: {
      "": false,
      w: false,
      we: false,
      wea: false,
      weat: false,
      weath: false,
      weathe: false,
      weather: false,
    },
  });
});

// * Settings data: keywords and min keyword length > 0
// * Nimbus values: none
// * Min keyword length pref: none
// * Expected: use settings data
add_task(async function () {
  await doKeywordsTest({
    desc: "Settings only, min keyword length > 0",
    settingsData: {
      keywords: ["weather"],
      min_keyword_length: 3,
    },
    tests: {
      "": false,
      w: false,
      we: false,
      wea: true,
      weat: true,
      weath: true,
      weathe: true,
      weather: true,
    },
  });
});

// * Settings data: keywords and min keyword length = 0
// * Nimbus values: none
// * Min keyword length pref: 6
// * Expected: no prefix matching because when min keyword length = 0, the Rust
//   component requires full keywords to be typed
add_task(async function () {
  await doKeywordsTest({
    desc: "Settings only, min keyword length = 0, pref exists",
    settingsData: {
      keywords: ["weather"],
      min_keyword_length: 0,
    },
    minKeywordLength: 6,
    tests: {
      "": false,
      w: false,
      we: false,
      wea: false,
      weat: false,
      weath: false,
      weathe: false,
      weather: true,
    },
  });
});

// * Settings data: keywords and min keyword length > 0
// * Nimbus values: none
// * Min keyword length pref: 6
// * Expected: use settings keywords and min keyword length pref
add_task(async function () {
  await doKeywordsTest({
    desc: "Settings only, min keyword length > 0, pref exists",
    settingsData: {
      keywords: ["weather"],
      min_keyword_length: 3,
    },
    minKeywordLength: 6,
    tests: {
      "": false,
      w: false,
      we: false,
      wea: false,
      weat: false,
      weath: false,
      weathe: true,
      weather: true,
    },
  });
});

// * Settings data: keywords and min keyword length > 0
// * Nimbus values: min keyword length = 0
// * Min keyword length pref: none
// * Expected: Settings min keyword length
add_task(async function () {
  await doKeywordsTest({
    desc: "Settings: keywords, min keyword length > 0; Nimbus: min keyword length = 0",
    settingsData: {
      keywords: ["weather"],
      min_keyword_length: 3,
    },
    nimbusValues: {
      weatherKeywordsMinimumLength: 0,
    },
    tests: {
      "": false,
      w: false,
      we: false,
      wea: true,
      weat: true,
      weath: true,
      weathe: true,
      weather: true,
    },
  });
});

// * Settings data: keywords and min keyword length > 0
// * Nimbus values: min keyword length > settings min keyword length
// * Min keyword length pref: none
// * Expected: Nimbus min keyword length
add_task(async function () {
  await doKeywordsTest({
    desc: "Settings: keywords, min keyword length > 0; Nimbus: min keyword length > settings min keyword length",
    settingsData: {
      keywords: ["weather"],
      min_keyword_length: 3,
    },
    nimbusValues: {
      weatherKeywordsMinimumLength: 4,
    },
    tests: {
      "": false,
      w: false,
      we: false,
      wea: false,
      weat: true,
      weath: true,
      weathe: true,
      weather: true,
    },
  });
});

// * Settings data: keywords and min keyword length > 0
// * Nimbus values: min keyword length = 0
// * Min keyword length pref: exists
// * Expected: Min keyword length pref
add_task(async function () {
  await doKeywordsTest({
    desc: "Settings: keywords, min keyword length > 0; Nimbus: min keyword length = 0; pref exists",
    settingsData: {
      keywords: ["weather"],
      min_keyword_length: 3,
    },
    nimbusValues: {
      weatherKeywordsMinimumLength: 0,
    },
    minKeywordLength: 6,
    tests: {
      "": false,
      w: false,
      we: false,
      wea: false,
      weat: false,
      weath: false,
      weathe: true,
      weather: true,
    },
  });
});

// * Settings data: keywords and min keyword length > 0
// * Nimbus values: min keyword length > settings min keyword length
// * Min keyword length pref: exists
// * Expected: min keyword length pref
add_task(async function () {
  await doKeywordsTest({
    desc: "Settings: keywords, min keyword length > 0; Nimbus: min keyword length > settings min keyword length; pref exists",
    settingsData: {
      keywords: ["weather"],
      min_keyword_length: 3,
    },
    nimbusValues: {
      weatherKeywordsMinimumLength: 4,
    },
    minKeywordLength: 6,
    tests: {
      "": false,
      w: false,
      we: false,
      wea: false,
      weat: false,
      weath: false,
      weathe: true,
      weather: true,
    },
  });
});

// * Settings data: keywords only
// * Nimbus values: min keyword length = 0
// * Min keyword length pref: none
// * Expected: Full keywords are required due to the Rust component
add_task(async function () {
  await doKeywordsTest({
    desc: "Settings: keywords only; Nimbus: min keyword length = 0",
    settingsData: {
      keywords: ["weather"],
    },
    nimbusValues: {
      weatherKeywordsMinimumLength: 0,
    },
    tests: {
      "": false,
      w: false,
      we: false,
      wea: false,
      weat: false,
      weath: false,
      weathe: false,
      weather: true,
    },
  });
});

// * Settings data: keywords only
// * Nimbus values: min keyword length > 0
// * Min keyword length pref: none
// * Expected: Full keywords are required due to the Rust component; the Nimbus
//   min-length check only happens after the Rust component has returned
//   suggestions
add_task(async function () {
  await doKeywordsTest({
    desc: "Settings: keywords only; Nimbus: min keyword length > 0",
    settingsData: {
      keywords: ["weather"],
    },
    nimbusValues: {
      weatherKeywordsMinimumLength: 4,
    },
    tests: {
      "": false,
      w: false,
      we: false,
      wea: false,
      weat: false,
      weath: false,
      weathe: false,
      weather: true,
    },
  });
});

// * Settings data: keywords only
// * Nimbus values: min keyword length = 0
// * Min keyword length pref: exists
// * Expected: Full keywords are required due to the Rust component; the pref
//   min-length check only happens after the Rust component has returned
//   suggestions; full keywords < min length pref are not matched
add_task(async function () {
  await doKeywordsTest({
    desc: "Settings: keywords only; Nimbus: min keyword length = 0; pref exists",
    settingsData: {
      keywords: ["weather", "wind"],
    },
    nimbusValues: {
      weatherKeywordsMinimumLength: 0,
    },
    minKeywordLength: 6,
    tests: {
      "": false,
      w: false,
      we: false,
      wea: false,
      weat: false,
      weath: false,
      weathe: false,
      weather: true,
      wi: false,
      win: false,
      wind: false,
    },
  });
});

// * Settings data: keywords and min keyword length > 0
// * Nimbus values: min keyword length > settings min keyword length
// * Min keyword length pref: exists
// * Expected: Full keywords are required due to the Rust component; the pref
//   and Nimbus min-length checks only happens after the Rust component has
//   returned suggestions; full keywords < min length pref are not matched
add_task(async function () {
  await doKeywordsTest({
    desc: "Settings: keywords only; Nimbus: min keyword length > settings min keyword length; pref exists",
    settingsData: {
      keywords: ["weather", "wind"],
    },
    nimbusValues: {
      weatherKeywordsMinimumLength: 4,
    },
    minKeywordLength: 6,
    tests: {
      "": false,
      w: false,
      we: false,
      wea: false,
      weat: false,
      weath: false,
      weathe: false,
      weather: true,
      wi: false,
      win: false,
      wind: false,
    },
  });
});

// Leading and trailing spaces should be ignored.
add_task(async function leadingAndTrailingSpaces() {
  await doKeywordsTest({
    settingsData: {
      keywords: ["weather"],
      min_keyword_length: 3,
    },
    tests: {
      " wea": true,
      "  wea": true,
      "wea ": true,
      "wea  ": true,
      "  wea  ": true,
      " weat": true,
      "  weat": true,
      "weat ": true,
      "weat  ": true,
      "  weat  ": true,
    },
  });
});

add_task(async function caseInsensitive() {
  await doKeywordsTest({
    desc: "Case insensitive",
    settingsData: {
      keywords: ["weather"],
      min_keyword_length: 3,
    },
    tests: {
      wea: true,
      WEA: true,
      Wea: true,
      WeA: true,
      WEATHER: true,
      Weather: true,
      WeAtHeR: true,
    },
  });
});

async function doKeywordsTest({
  desc,
  tests,
  nimbusValues = null,
  settingsData = null,
  minKeywordLength = undefined,
}) {
  info("Doing keywords test: " + desc);
  info(JSON.stringify({ nimbusValues, settingsData, minKeywordLength }));

  let nimbusCleanup;
  if (nimbusValues) {
    nimbusCleanup = await UrlbarTestUtils.initNimbusFeature(nimbusValues);
  }

  let records = [];
  if (settingsData) {
    records.push(QuickSuggestTestUtils.weatherRecord(settingsData));
  }
  await QuickSuggestTestUtils.setRemoteSettingsRecords(records);

  if (minKeywordLength) {
    UrlbarPrefs.set("weather.minKeywordLength", minKeywordLength);
  }

  let expectedResult = QuickSuggestTestUtils.weatherResult();

  for (let [searchString, expected] of Object.entries(tests)) {
    info(
      "Doing keywords test search: " +
        JSON.stringify({
          searchString,
          expected,
        })
    );

    await check_results({
      context: createContext(searchString, {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
      matches: expected ? [expectedResult] : [],
    });
  }

  await nimbusCleanup?.();

  await QuickSuggestTestUtils.setRemoteSettingsRecords([
    QuickSuggestTestUtils.weatherRecord(),
  ]);

  UrlbarPrefs.clear("weather.minKeywordLength");
}

// Tests the "Show less frequently" command when no show-less-frequently cap is
// defined.
add_task(async function () {
  await doShowLessFrequentlyTest({
    desc: "No cap",
    weather: {
      keywords: ["forecast"],
      min_keyword_length: 3,
    },
    tests: [
      {
        input: "for",
        before: {
          canShowLessFrequently: true,
          showLessFrequentlyCount: 0,
          minKeywordLength: 0,
        },
        after: {
          canShowLessFrequently: true,
          showLessFrequentlyCount: 1,
          minKeywordLength: 4,
        },
      },
      {
        input: "fore",
        before: {
          canShowLessFrequently: true,
          showLessFrequentlyCount: 1,
          minKeywordLength: 4,
        },
        after: {
          canShowLessFrequently: true,
          showLessFrequentlyCount: 2,
          minKeywordLength: 5,
        },
      },
      {
        input: "forecast",
        before: {
          canShowLessFrequently: true,
          showLessFrequentlyCount: 2,
          minKeywordLength: 5,
        },
        after: {
          canShowLessFrequently: true,
          showLessFrequentlyCount: 3,
          minKeywordLength: 9,
        },
      },
    ],
  });
});

// Tests the "Show less frequently" command when the show-less-frequently cap is
// defined in the remote settings config, Nimbus, or both.
add_task(async function () {
  for (let configuration of [null, { show_less_frequently_cap: 3 }]) {
    for (let nimbusValues of [null, { weatherShowLessFrequentlyCap: 3 }]) {
      if (!configuration && !nimbusValues) {
        continue;
      }
      await doShowLessFrequentlyTest({
        desc: "Cap: " + JSON.stringify({ configuration, nimbusValues }),
        weather: {
          keywords: ["forecast"],
          min_keyword_length: 3,
        },
        configuration,
        nimbusValues,
        tests: [
          {
            input: "for",
            before: {
              canShowLessFrequently: true,
              showLessFrequentlyCount: 0,
              minKeywordLength: 0,
            },
            after: {
              canShowLessFrequently: true,
              showLessFrequentlyCount: 1,
              minKeywordLength: 4,
            },
          },
          {
            input: "fore",
            before: {
              canShowLessFrequently: true,
              showLessFrequentlyCount: 1,
              minKeywordLength: 4,
            },
            after: {
              canShowLessFrequently: true,
              showLessFrequentlyCount: 2,
              minKeywordLength: 5,
            },
          },
          {
            input: "forecast",
            before: {
              canShowLessFrequently: true,
              showLessFrequentlyCount: 2,
              minKeywordLength: 5,
            },
            after: {
              // Shouldn't be able to show less frequently now.
              canShowLessFrequently: false,
              showLessFrequentlyCount: 3,
              minKeywordLength: 9,
            },
          },
        ],
      });
    }
  }
});

async function doShowLessFrequentlyTest({
  desc,
  tests,
  weather,
  configuration = null,
  nimbusValues = null,
}) {
  info("Doing increment test: " + desc);
  info(JSON.stringify({ weather, configuration, nimbusValues }));

  let nimbusCleanup;
  if (nimbusValues) {
    nimbusCleanup = await UrlbarTestUtils.initNimbusFeature(nimbusValues);
  }

  let records = [QuickSuggestTestUtils.weatherRecord(weather)];
  if (configuration) {
    records.push({
      type: "configuration",
      configuration,
    });
  }
  await QuickSuggestTestUtils.setRemoteSettingsRecords(records);

  let feature = QuickSuggest.getFeature("WeatherSuggestions");
  let expectedResult = QuickSuggestTestUtils.weatherResult();

  for (let { input, before, after } of tests) {
    info("Doing increment test case: " + JSON.stringify({ input }));

    await check_results({
      context: createContext(input, {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
      matches: [expectedResult],
    });

    Assert.equal(
      UrlbarPrefs.get("weather.minKeywordLength"),
      before.minKeywordLength,
      "weather.minKeywordLength before"
    );
    Assert.equal(
      feature.canShowLessFrequently,
      before.canShowLessFrequently,
      "feature.canShowLessFrequently before"
    );
    Assert.equal(
      feature.showLessFrequentlyCount,
      before.showLessFrequentlyCount,
      "feature.showLessFrequentlyCount before"
    );

    triggerCommand({
      feature,
      result: expectedResult,
      command: "show_less_frequently",
      searchString: input,
    });

    Assert.equal(
      UrlbarPrefs.get("weather.minKeywordLength"),
      after.minKeywordLength,
      "weather.minKeywordLength after"
    );
    Assert.equal(
      feature.canShowLessFrequently,
      after.canShowLessFrequently,
      "feature.canShowLessFrequently after"
    );
    Assert.equal(
      feature.showLessFrequentlyCount,
      after.showLessFrequentlyCount,
      "feature.showLessFrequentlyCount after"
    );

    await check_results({
      context: createContext(input, {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
      matches: [],
    });
  }

  await nimbusCleanup?.();

  await QuickSuggestTestUtils.setRemoteSettingsRecords([
    QuickSuggestTestUtils.weatherRecord(),
  ]);
  UrlbarPrefs.clear("weather.minKeywordLength");
  UrlbarPrefs.clear("weather.showLessFrequentlyCount");
}
