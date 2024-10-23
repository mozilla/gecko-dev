/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the keywords behavior of quick suggest weather.

"use strict";

add_setup(async () => {
  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    remoteSettingsRecords: [QuickSuggestTestUtils.weatherRecord()],
    prefs: [
      ["suggest.quicksuggest.nonsponsored", true],
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
// * Nimbus values: min keyword length > 0
// * Min keyword length pref: none
// * Expected: Nimbus min keyword length
add_task(async function () {
  await doKeywordsTest({
    desc: "Settings: keywords, min keyword length > 0; Nimbus: min keyword length > 0",
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
// * Nimbus values: min keyword length > 0
// * Min keyword length pref: exists
// * Expected: min keyword length pref
add_task(async function () {
  await doKeywordsTest({
    desc: "Settings: keywords, min keyword length > 0; Nimbus: min keyword length > 0; pref exists",
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

add_task(async function () {
  await doIncrementTest({
    desc: "Settings only without cap",
    weather: {
      keywords: ["forecast", "wind"],
      min_keyword_length: 3,
    },
    tests: [
      {
        minKeywordLength: 3,
        canIncrement: true,
        searches: {
          fo: false,
          for: true,
          fore: true,
          forec: true,
          wi: false,
          win: true,
          wind: true,
        },
      },
      {
        minKeywordLength: 4,
        canIncrement: true,
        searches: {
          fo: false,
          for: false,
          fore: true,
          forec: true,
          wi: false,
          win: false,
          wind: true,
        },
      },
      {
        minKeywordLength: 5,
        canIncrement: true,
        searches: {
          fo: false,
          for: false,
          fore: false,
          forec: true,
          wi: false,
          win: false,
          wind: false,
        },
      },
    ],
  });
});

add_task(async function () {
  await doIncrementTest({
    desc: "Settings only with cap",
    weather: {
      keywords: ["forecast", "wind"],
      min_keyword_length: 3,
    },
    configuration: {
      show_less_frequently_cap: 3,
    },
    tests: [
      {
        minKeywordLength: 3,
        canIncrement: true,
        searches: {
          fo: false,
          for: true,
          fore: true,
          forec: true,
          foreca: true,
          forecas: true,
          wi: false,
          win: true,
          wind: true,
        },
      },
      {
        minKeywordLength: 4,
        canIncrement: true,
        searches: {
          fo: false,
          for: false,
          fore: true,
          forec: true,
          foreca: true,
          forecas: true,
          wi: false,
          win: false,
          wind: true,
        },
      },
      {
        minKeywordLength: 5,
        canIncrement: true,
        searches: {
          fo: false,
          for: false,
          fore: false,
          forec: true,
          foreca: true,
          forecas: true,
          wi: false,
          win: false,
          wind: false,
          windy: false,
        },
      },
      {
        minKeywordLength: 6,
        canIncrement: false,
        searches: {
          fo: false,
          for: false,
          fore: false,
          forec: false,
          foreca: true,
          forecas: true,
          wi: false,
          win: false,
          wind: false,
          windy: false,
        },
      },
      {
        minKeywordLength: 6,
        canIncrement: false,
        searches: {
          fo: false,
          for: false,
          fore: false,
          forec: false,
          foreca: true,
          forecas: true,
          wi: false,
          win: false,
          wind: false,
          windy: false,
        },
      },
    ],
  });
});

add_task(async function () {
  await doIncrementTest({
    desc: "Settings and Nimbus without cap",
    weather: {
      keywords: ["weather"],
      min_keyword_length: 5,
    },
    nimbusValues: {
      weatherKeywordsMinimumLength: 3,
    },
    // The Rust component will use the min keyword length in the RS config. The
    // Nimbus value will be ignored.
    tests: [
      {
        minKeywordLength: 3,
        canIncrement: true,
        searches: {
          we: false,
          wea: false,
          weat: false,
          weath: true,
          weathe: true,
        },
      },
      {
        minKeywordLength: 4,
        canIncrement: true,
        searches: {
          we: false,
          wea: false,
          weat: false,
          weath: true,
          weathe: true,
        },
      },
      {
        minKeywordLength: 5,
        canIncrement: true,
        searches: {
          we: false,
          wea: false,
          weat: false,
          weath: true,
          weathe: true,
        },
      },
      {
        minKeywordLength: 6,
        canIncrement: true,
        searches: {
          we: false,
          wea: false,
          weat: false,
          weath: false,
          weathe: true,
        },
      },
    ],
  });
});

add_task(async function () {
  await doIncrementTest({
    desc: "Settings and Nimbus with cap in Nimbus",
    weather: {
      keywords: ["weather"],
      min_keyword_length: 5,
    },
    nimbusValues: {
      weatherKeywordsMinimumLength: 3,
      weatherKeywordsMinimumLengthCap: 6,
    },
    // The Rust component will use the min keyword length in the RS config. The
    // Nimbus value will be ignored.
    tests: [
      {
        minKeywordLength: 3,
        canIncrement: true,
        searches: {
          we: false,
          wea: false,
          weat: false,
          weath: true,
          weathe: true,
        },
      },
      {
        minKeywordLength: 4,
        canIncrement: true,
        searches: {
          we: false,
          wea: false,
          weat: false,
          weath: true,
          weathe: true,
        },
      },
      {
        minKeywordLength: 5,
        canIncrement: true,
        searches: {
          we: false,
          wea: false,
          weat: false,
          weath: true,
          weathe: true,
        },
      },
      {
        minKeywordLength: 6,
        canIncrement: false,
        searches: {
          we: false,
          wea: false,
          weat: false,
          weath: false,
          weathe: true,
        },
      },
      {
        minKeywordLength: 6,
        canIncrement: false,
        searches: {
          we: false,
          wea: false,
          weat: false,
          weath: false,
          weathe: true,
        },
      },
    ],
  });
});

async function doIncrementTest({
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

  let expectedResult = QuickSuggestTestUtils.weatherResult();

  for (let { minKeywordLength, canIncrement, searches } of tests) {
    info(
      "Doing increment test case: " +
        JSON.stringify({
          minKeywordLength,
          canIncrement,
        })
    );

    Assert.equal(
      QuickSuggest.weather.minKeywordLength,
      minKeywordLength,
      "minKeywordLength should be correct"
    );
    Assert.equal(
      QuickSuggest.weather.canIncrementMinKeywordLength,
      canIncrement,
      "canIncrement should be correct"
    );

    for (let [searchString, expected] of Object.entries(searches)) {
      await check_results({
        context: createContext(searchString, {
          providers: [UrlbarProviderQuickSuggest.name],
          isPrivate: false,
        }),
        matches: expected ? [expectedResult] : [],
      });
    }

    QuickSuggest.weather.incrementMinKeywordLength();
    info(
      "Incremented min keyword length, new value is: " +
        QuickSuggest.weather.minKeywordLength
    );
  }

  await nimbusCleanup?.();

  await QuickSuggestTestUtils.setRemoteSettingsRecords([
    QuickSuggestTestUtils.weatherRecord(),
  ]);
  UrlbarPrefs.clear("weather.minKeywordLength");
}
