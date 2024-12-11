/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests addon quick suggest results.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  AddonTestUtils: "resource://testing-common/AddonTestUtils.sys.mjs",
  ExtensionTestCommon: "resource://testing-common/ExtensionTestCommon.sys.mjs",
});

AddonTestUtils.init(this, false);
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "42",
  "42"
);

// TODO: Firefox no longer uses `rating` and `number_of_ratings` but they are
// still present in Merino and RS suggestions, so they are included here for
// greater accuracy. We should remove them from Merino, RS, and tests.
const MERINO_SUGGESTIONS = [
  {
    provider: "amo",
    icon: "icon",
    url: "https://example.com/merino-addon",
    title: "title",
    description: "description",
    is_top_pick: true,
    custom_details: {
      amo: {
        rating: "5",
        number_of_ratings: "1234567",
        guid: "test@addon",
      },
    },
  },
];

const REMOTE_SETTINGS_RESULTS = [
  {
    type: "amo-suggestions",
    attachment: [
      {
        url: "https://example.com/first-addon",
        guid: "first@addon",
        icon: "https://example.com/first-addon.svg",
        title: "First Addon",
        rating: "4.7",
        keywords: ["first", "1st", "two words", "aa b c"],
        description: "Description for the First Addon",
        number_of_ratings: 1256,
        score: 0.25,
      },
      {
        url: "https://example.com/second-addon",
        guid: "second@addon",
        icon: "https://example.com/second-addon.svg",
        title: "Second Addon",
        rating: "1.7",
        keywords: ["second", "2nd"],
        description: "Description for the Second Addon",
        number_of_ratings: 256,
        score: 0.25,
      },
      {
        url: "https://example.com/third-addon",
        guid: "third@addon",
        icon: "https://example.com/third-addon.svg",
        title: "Third Addon",
        rating: "3.7",
        keywords: ["third", "3rd"],
        description: "Description for the Third Addon",
        number_of_ratings: 3,
        score: 0.25,
      },
      {
        url: "https://example.com/fourth-addon?utm_medium=aaa&utm_source=bbb",
        guid: "fourth@addon",
        icon: "https://example.com/fourth-addon.svg",
        title: "Fourth Addon",
        rating: "4.7",
        keywords: ["fourth", "4th"],
        description: "Description for the Fourth Addon",
        number_of_ratings: 4,
        score: 0.25,
      },
    ],
  },
];

add_setup(async function init() {
  await AddonTestUtils.promiseStartupManager();

  // Disable search suggestions so we don't hit the network.
  Services.prefs.setBoolPref("browser.search.suggest.enabled", false);

  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    remoteSettingsRecords: REMOTE_SETTINGS_RESULTS,
    merinoSuggestions: MERINO_SUGGESTIONS,
    prefs: [["suggest.quicksuggest.nonsponsored", true]],
  });
});

add_task(async function telemetryType() {
  Assert.equal(
    QuickSuggest.getFeature("AddonSuggestions").getSuggestionTelemetryType({}),
    "amo",
    "Telemetry type should be 'amo'"
  );
});

// When quick suggest prefs are disabled, addon suggestions should be disabled.
add_task(async function quickSuggestPrefsDisabled() {
  let prefs = ["quicksuggest.enabled", "suggest.quicksuggest.nonsponsored"];
  for (let pref of prefs) {
    // Before disabling the pref, first make sure the suggestion is added.
    await check_results({
      context: createContext("test", {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
      matches: [
        makeExpectedResult({
          suggestion: MERINO_SUGGESTIONS[0],
          source: "merino",
          provider: "amo",
        }),
      ],
    });

    // Now disable the pref.
    UrlbarPrefs.set(pref, false);
    await check_results({
      context: createContext("test", {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
      matches: [],
    });

    UrlbarPrefs.set(pref, true);
    await QuickSuggestTestUtils.forceSync();
  }
});

// When addon suggestions specific preference is disabled, addon suggestions
// should not be added.
add_task(async function addonSuggestionsSpecificPrefDisabled() {
  const prefs = ["suggest.addons", "addons.featureGate"];
  for (const pref of prefs) {
    // First make sure the suggestion is added.
    await check_results({
      context: createContext("test", {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
      matches: [
        makeExpectedResult({
          suggestion: MERINO_SUGGESTIONS[0],
          source: "merino",
          provider: "amo",
        }),
      ],
    });

    // Now disable the pref.
    UrlbarPrefs.set(pref, false);
    await check_results({
      context: createContext("test", {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
      matches: [],
    });

    // Revert.
    UrlbarPrefs.clear(pref);
    await QuickSuggestTestUtils.forceSync();
  }
});

// Check wheather the addon suggestions will be shown by the setup of Nimbus
// variable.
add_task(async function nimbus() {
  // Disable the fature gate.
  UrlbarPrefs.set("addons.featureGate", false);
  await check_results({
    context: createContext("test", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [],
  });

  // Enable by Nimbus.
  const cleanUpNimbusEnable = await UrlbarTestUtils.initNimbusFeature({
    addonsFeatureGate: true,
  });
  await QuickSuggestTestUtils.forceSync();
  await check_results({
    context: createContext("test", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [
      makeExpectedResult({
        suggestion: MERINO_SUGGESTIONS[0],
        source: "merino",
        provider: "amo",
      }),
    ],
  });
  await cleanUpNimbusEnable();

  // Enable locally.
  UrlbarPrefs.set("addons.featureGate", true);
  await QuickSuggestTestUtils.forceSync();

  // Disable by Nimbus.
  const cleanUpNimbusDisable = await UrlbarTestUtils.initNimbusFeature({
    addonsFeatureGate: false,
  });
  await check_results({
    context: createContext("test", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [],
  });
  await cleanUpNimbusDisable();

  // Revert.
  UrlbarPrefs.clear("addons.featureGate");
  await QuickSuggestTestUtils.forceSync();
});

add_task(async function hideIfAlreadyInstalled() {
  // Show suggestion.
  await check_results({
    context: createContext("test", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [
      makeExpectedResult({
        suggestion: MERINO_SUGGESTIONS[0],
        source: "merino",
        provider: "amo",
      }),
    ],
  });

  // Install an addon for the suggestion.
  const xpi = ExtensionTestCommon.generateXPI({
    manifest: {
      browser_specific_settings: {
        gecko: { id: "test@addon" },
      },
    },
  });
  const addon = await AddonManager.installTemporaryAddon(xpi);

  // Show suggestion for the addon installed.
  await check_results({
    context: createContext("test", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [],
  });

  await addon.uninstall();
  xpi.remove(false);
});

add_task(async function remoteSettings() {
  const testCases = [
    {
      input: "f",
      expected: null,
    },
    {
      input: "fi",
      expected: null,
    },
    {
      input: "fir",
      expected: null,
    },
    {
      input: "firs",
      expected: null,
    },
    {
      input: "first",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[0],
      }),
    },
    {
      input: "1st",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[0],
      }),
    },
    {
      input: "t",
      expected: null,
    },
    {
      input: "tw",
      expected: null,
    },
    {
      input: "two",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[0],
      }),
    },
    {
      input: "two ",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[0],
      }),
    },
    {
      input: "two w",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[0],
      }),
    },
    {
      input: "two wo",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[0],
      }),
    },
    {
      input: "two wor",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[0],
      }),
    },
    {
      input: "two word",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[0],
      }),
    },
    {
      input: "two words",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[0],
      }),
    },
    {
      input: "aa",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[0],
      }),
    },
    {
      input: "aa ",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[0],
      }),
    },
    {
      input: "aa b",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[0],
      }),
    },
    {
      input: "aa b ",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[0],
      }),
    },
    {
      input: "aa b c",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[0],
      }),
    },
    {
      input: "second",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[1],
      }),
    },
    {
      input: "2nd",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[1],
      }),
    },
    {
      input: "third",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[2],
      }),
    },
    {
      input: "3rd",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[2],
      }),
    },
    {
      input: "fourth",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[3],
        setUtmParams: false,
      }),
    },
    {
      input: "FoUrTh",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[3],
        setUtmParams: false,
      }),
    },
  ];

  // Disable Merino so we trigger only remote settings suggestions.
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", false);

  for (let { input, expected } of testCases) {
    await check_results({
      context: createContext(input, {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
      matches: expected ? [expected] : [],
    });
  }

  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", true);
});

add_task(async function merinoIsTopPick() {
  const suggestion = JSON.parse(JSON.stringify(MERINO_SUGGESTIONS[0]));

  // is_top_pick is specified as false.
  suggestion.is_top_pick = false;
  MerinoTestUtils.server.response.body.suggestions = [suggestion];
  await check_results({
    context: createContext("test", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [
      makeExpectedResult({
        suggestion,
        source: "merino",
        provider: "amo",
      }),
    ],
  });

  // is_top_pick is undefined.
  delete suggestion.is_top_pick;
  MerinoTestUtils.server.response.body.suggestions = [suggestion];
  await check_results({
    context: createContext("test", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [
      makeExpectedResult({
        suggestion,
        source: "merino",
        provider: "amo",
      }),
    ],
  });
});

// Tests the "show less frequently" behavior.
add_task(async function showLessFrequently() {
  await doShowLessFrequentlyTests({
    feature: QuickSuggest.getFeature("AddonSuggestions"),
    showLessFrequentlyCountPref: "addons.showLessFrequentlyCount",
    nimbusCapVariable: "addonsShowLessFrequentlyCap",
    expectedResult: makeExpectedResult({
      suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[0],
    }),
    keyword: "two words",
  });
});

// The `Amo` Rust provider should be passed to the Rust component when querying
// depending on whether addon suggestions are enabled.
add_task(async function rustProviders() {
  await doRustProvidersTests({
    searchString: "first",
    tests: [
      {
        prefs: {
          "suggest.addons": true,
        },
        expectedUrls: ["https://example.com/first-addon"],
      },
      {
        prefs: {
          "suggest.addons": false,
        },
        expectedUrls: [],
      },
    ],
  });

  UrlbarPrefs.clear("suggest.addons");
  await QuickSuggestTestUtils.forceSync();
});

function makeExpectedResult({
  suggestion,
  source,
  provider,
  setUtmParams = true,
}) {
  return QuickSuggestTestUtils.amoResult({
    source,
    provider,
    setUtmParams,
    title: suggestion.title,
    description: suggestion.description,
    url: suggestion.url,
    originalUrl: suggestion.url,
    icon: suggestion.icon,
  });
}
