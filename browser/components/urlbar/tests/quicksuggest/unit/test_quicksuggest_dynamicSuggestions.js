/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests dynamic Rust suggestions.

const REMOTE_SETTINGS_RECORDS = [
  {
    type: "dynamic-suggestions",
    suggestion_type: "aaa",
    score: 0.9,
    attachment: [
      {
        keywords: ["aaa keyword", "aaa bbb keyword", "wikipedia"],
        data: {
          result: {
            payload: {
              title: "aaa title",
              url: "https://example.com/aaa",
            },
          },
        },
      },
    ],
  },
  {
    type: "dynamic-suggestions",
    suggestion_type: "bbb",
    score: 0.1,
    attachment: [
      {
        keywords: ["bbb keyword", "aaa bbb keyword", "wikipedia"],
        dismissal_key: "bbb-dismissal-key",
        data: {
          result: {
            isBestMatch: true,
            suggestedIndex: 1,
            isSuggestedIndexRelativeToGroup: false,
            isRichSuggestion: true,
            payload: {
              title: "bbb title",
              url: "https://example.com/bbb",
              isSponsored: true,
              telemetryType: "bbb_telemetry_type",
            },
          },
        },
      },
    ],
  },
  {
    type: QuickSuggestTestUtils.RS_TYPE.WIKIPEDIA,
    attachment: [QuickSuggestTestUtils.wikipediaRemoteSettings()],
  },
];

const EXPECTED_AAA_RESULT = makeExpectedResult({
  title: "aaa title",
  url: "https://example.com/aaa",
  telemetryType: "aaa",
});

const EXPECTED_BBB_RESULT = makeExpectedResult({
  title: "bbb title",
  url: "https://example.com/bbb",
  isSponsored: true,
  telemetryType: "bbb_telemetry_type",
  isBestMatch: true,
  suggestedIndex: 1,
  isSuggestedIndexRelativeToGroup: false,
  isRichSuggestion: true,
});

add_setup(async function () {
  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    remoteSettingsRecords: REMOTE_SETTINGS_RECORDS,
    prefs: [
      ["quicksuggest.dynamicSuggestionTypes", "aaa,bbb"],
      ["suggest.quicksuggest.sponsored", true],
      ["suggest.quicksuggest.nonsponsored", true],
      ["quicksuggest.ampTopPickCharThreshold", 0],
    ],
  });
});

// When a dynamic suggestion doesn't include `telemetryType`, its
// `suggestionType` should be used as the telemetry type.
add_task(async function telemetryType_default() {
  Assert.equal(
    QuickSuggest.getFeature("DynamicSuggestions").getSuggestionTelemetryType({
      suggestionType: "abcdefg",
    }),
    "abcdefg",
    "Telemetry type should be correct when using default"
  );
});

// When a dynamic suggestion includes `telemetryType`, it should be used as the
// telemetry type.
add_task(async function telemetryType_override() {
  Assert.equal(
    QuickSuggest.getFeature("DynamicSuggestions").getSuggestionTelemetryType({
      suggestionType: "abcdefg",
      data: {
        result: {
          payload: {
            telemetryType: "telemetry_type_override",
          },
        },
      },
    }),
    "telemetry_type_override",
    "Telemetry type should be correct when overridden"
  );
});

add_task(async function basic() {
  let queries = [
    {
      query: "no match",
      expected: [],
    },
    {
      query: "aaa keyword",
      expected: [EXPECTED_AAA_RESULT],
    },
    {
      query: "bbb keyword",
      expected: [EXPECTED_BBB_RESULT],
    },
    {
      query: "aaa bbb keyword",
      // The "aaa" suggestion has a higher score than "bbb".
      expected: [EXPECTED_AAA_RESULT],
    },
  ];

  await doQueries(queries);
});

// When only one dynamic suggestion type is enabled, only its result should be
// returned. This task assumes multiples types were added to remote settings in
// the setup task.
add_task(async function oneSuggestionType() {
  await withSuggestionTypesPref("bbb", async () => {
    await doQueries([
      {
        query: "aaa keyword",
        expected: [],
      },
      {
        query: "bbb keyword",
        expected: [EXPECTED_BBB_RESULT],
      },
      {
        query: "aaa bbb keyword",
        expected: [EXPECTED_BBB_RESULT],
      },
      {
        query: "doesn't match",
        expected: [],
      },
    ]);
  });
});

// When no dynamic suggestion types are enabled, no results should be added.
add_task(async function disabled() {
  await withSuggestionTypesPref("", async () => {
    await doQueries(
      ["aaa keyword", "bbb keyword", "aaa bbb keyword"].map(query => ({
        query,
        expected: [],
      }))
    );
  });
});

// Dynamic suggestions that are sponsored shouldn't be added when sponsored
// suggestions are disabled.
add_task(async function sponsoredDisabled() {
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", false);

  // Enable both "aaa" (nonsponsored) and "bbb" (sponsored).
  await withSuggestionTypesPref("aaa,bbb", async () => {
    await doQueries([
      {
        query: "aaa keyword",
        expected: [EXPECTED_AAA_RESULT],
      },
      {
        query: "bbb keyword",
        expected: [],
      },
      {
        query: "aaa bbb keyword",
        expected: [EXPECTED_AAA_RESULT],
      },
    ]);
  });

  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);
  await QuickSuggestTestUtils.forceSync();
});

// Dynamic suggestions that are nonsponsored shouldn't be added when
// nonsponsored suggestions are disabled.
add_task(async function sponsoredDisabled() {
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", false);

  // Enable both "aaa" (nonsponsored) and "bbb" (sponsored).
  await withSuggestionTypesPref("aaa,bbb", async () => {
    await doQueries([
      {
        query: "aaa keyword",
        expected: [],
      },
      {
        query: "bbb keyword",
        expected: [EXPECTED_BBB_RESULT],
      },
      {
        query: "aaa bbb keyword",
        expected: [EXPECTED_BBB_RESULT],
      },
    ]);
  });

  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  await QuickSuggestTestUtils.forceSync();
});

// Tests the `quickSuggestDynamicSuggestionTypes` Nimbus variable.
add_task(async function nimbus() {
  // Clear `dynamicSuggestionTypes` to make sure the value comes from the Nimbus
  // variable and not the pref.
  await withSuggestionTypesPref("", async () => {
    let cleanup = await UrlbarTestUtils.initNimbusFeature({
      quickSuggestDynamicSuggestionTypes: "aaa,bbb",
    });
    await QuickSuggestTestUtils.forceSync();
    await doQueries([
      {
        query: "aaa keyword",
        expected: [EXPECTED_AAA_RESULT],
      },
      {
        query: "bbb keyword",
        expected: [EXPECTED_BBB_RESULT],
      },
      {
        query: "aaa bbb keyword",
        // The "aaa" suggestion has a higher score than "bbb".
        expected: [EXPECTED_AAA_RESULT],
      },
      {
        query: "doesn't match",
        expected: [],
      },
    ]);

    await cleanup();
  });
});

// Tests dismissals. Note that dynamic suggestions must define a `dismissal_key`
// in order to be dismissable.
add_task(async function dismissal() {
  // Do a search and get the actual result that's returned.
  let context = createContext("bbb keyword", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_BBB_RESULT],
  });

  let result = context.results[0];
  let { suggestionObject } = result.payload;
  let { dismissalKey } = suggestionObject;
  Assert.equal(
    dismissalKey,
    "bbb-dismissal-key",
    "The suggestion should have the expected dismissal key"
  );

  // It shouldn't be dismissed yet.
  Assert.ok(
    !(await QuickSuggest.isResultDismissed(result)),
    "The result should not be dismissed yet"
  );
  Assert.ok(
    !(await QuickSuggest.rustBackend.isRustSuggestionDismissed(
      suggestionObject
    )),
    "The suggestion should not be dismissed yet"
  );
  Assert.ok(
    !(await QuickSuggest.rustBackend.isDismissedByKey(dismissalKey)),
    "The dismissal key should not be registered yet"
  );

  // Dismiss it. It should be dismissed by its dismissal key.
  await QuickSuggest.dismissResult(result);

  Assert.ok(
    await QuickSuggest.isResultDismissed(result),
    "The result should be dismissed"
  );
  Assert.ok(
    await QuickSuggest.rustBackend.isRustSuggestionDismissed(suggestionObject),
    "The suggestion should be dismissed"
  );
  Assert.ok(
    await QuickSuggest.rustBackend.isDismissedByKey(dismissalKey),
    "The dismissal key should be registered"
  );

  await check_results({
    context: createContext("bbb keyword", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [],
  });

  // Clear dismissals and check again.
  await QuickSuggest.clearDismissedSuggestions();

  await check_results({
    context: createContext("bbb keyword", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [EXPECTED_BBB_RESULT],
  });

  Assert.ok(
    !(await QuickSuggest.isResultDismissed(result)),
    "The result should not be dismissed after clearing dismissals"
  );
  Assert.ok(
    !(await QuickSuggest.rustBackend.isRustSuggestionDismissed(
      suggestionObject
    )),
    "The suggestion should not be dismissed after clearing dismissals"
  );
  Assert.ok(
    !(await QuickSuggest.rustBackend.isDismissedByKey(dismissalKey)),
    "The dismissal key should not be registered after clearing dismissals"
  );
});

// Tests some suggestions with bad data that desktop ignores.
add_task(async function badSuggestions() {
  await QuickSuggestTestUtils.setRemoteSettingsRecords([
    {
      type: "dynamic-suggestions",
      suggestion_type: "bad",
      attachment: [
        // Include a good suggestion so we can verify this record was actually
        // ingested.
        REMOTE_SETTINGS_RECORDS[0].attachment[0],
        // `data` is missing -- Rust actually allows this since `data` is
        // defined as `Option<serde_json::Value>`, but desktop doesn't.
        {
          keywords: ["bad"],
        },
        // `data` isn't an object
        {
          data: 123,
          keywords: ["bad"],
        },
        // `data.result` is missing
        {
          data: {},
          keywords: ["bad"],
        },
        // `data.result` isn't an object
        {
          data: {
            result: 123,
          },
          keywords: ["bad"],
        },
        // `data.result.payload` isn't an object
        {
          data: {
            result: {
              payload: 123,
            },
          },
          keywords: ["bad"],
        },
      ],
    },
  ]);

  await withSuggestionTypesPref("bad", async () => {
    // Verify the good suggestion was ingested.
    await check_results({
      context: createContext("aaa keyword", {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
      matches: [
        {
          ...EXPECTED_AAA_RESULT,
          payload: {
            ...EXPECTED_AAA_RESULT.payload,
            telemetryType: "bad",
          },
        },
      ],
    });

    // No "bad" suggestions should be matched.
    await check_results({
      context: createContext("bad", {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
      matches: [],
    });
  });

  // Clean up.
  await QuickSuggestTestUtils.setRemoteSettingsRecords(REMOTE_SETTINGS_RECORDS);
});

async function doQueries(queries) {
  for (let { query, expected } of queries) {
    info(
      "Doing query: " +
        JSON.stringify({
          query,
          expected,
        })
    );

    await check_results({
      context: createContext(query, {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
      matches: expected,
    });
  }
}

async function withSuggestionTypesPref(prefValue, callback) {
  // Use `Services` to get the original pref value since `UrlbarPrefs` will
  // parse the string value into a `Set`.
  let originalPrefValue = Services.prefs.getCharPref(
    "browser.urlbar.quicksuggest.dynamicSuggestionTypes"
  );

  // Changing the pref (or Nimbus variable) to a different value will trigger
  // ingest, so force sync afterward (or at least wait for ingest to finish).
  UrlbarPrefs.set("quicksuggest.dynamicSuggestionTypes", prefValue);
  await QuickSuggestTestUtils.forceSync();

  await callback();

  UrlbarPrefs.set("quicksuggest.dynamicSuggestionTypes", originalPrefValue);
  await QuickSuggestTestUtils.forceSync();
}

function makeExpectedResult({
  title,
  url,
  telemetryType,
  isSponsored = false,
  isBestMatch = false,
  suggestedIndex = -1,
  isSuggestedIndexRelativeToGroup = true,
  isRichSuggestion = undefined,
}) {
  return {
    type: UrlbarUtils.RESULT_TYPE.URL,
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    heuristic: false,
    isBestMatch,
    suggestedIndex,
    isRichSuggestion,
    isSuggestedIndexRelativeToGroup,
    payload: {
      title,
      url,
      isSponsored,
      telemetryType,
      displayUrl: url.replace(/^https:\/\//, ""),
      source: "rust",
      provider: "Dynamic",
      isManageable: true,
      helpUrl: QuickSuggest.HELP_URL,
    },
  };
}
