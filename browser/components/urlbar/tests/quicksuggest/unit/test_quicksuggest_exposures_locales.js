/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Checks that exposure suggestions can be enabled in Nimbus experiments
// regardless of region and locale, even for regions and locales where Suggest
// is normally disabled.

"use strict";

const REMOTE_SETTINGS_RECORDS = [
  {
    type: "exposure-suggestions",
    suggestion_type: "aaa",
    attachment: {
      keywords: ["aaa keyword", "aaa bbb keyword", "amp", "wikipedia"],
    },
  },
  {
    type: "exposure-suggestions",
    suggestion_type: "bbb",
    attachment: {
      keywords: ["bbb keyword", "aaa bbb keyword", "amp", "wikipedia"],
    },
  },
  {
    collection: QuickSuggestTestUtils.RS_COLLECTION.AMP,
    type: QuickSuggestTestUtils.RS_TYPE.AMP,
    attachment: [QuickSuggestTestUtils.ampRemoteSettings()],
  },
  {
    type: QuickSuggestTestUtils.RS_TYPE.WIKIPEDIA,
    attachment: [QuickSuggestTestUtils.wikipediaRemoteSettings()],
  },
];

add_setup(async function () {
  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    remoteSettingsRecords: REMOTE_SETTINGS_RECORDS,
  });

  // `ensureQuickSuggestInit()` enabled Suggest, but we want to start with it
  // disabled so that when we change locales, we can verify Suggest is properly
  // disabled or enabled depending on the locale.
  UrlbarPrefs.clear("quicksuggest.enabled");
});

add_task(async function suggestEnabledLocales() {
  let tests = [
    {
      homeRegion: "US",
      locales: ["en-US", "en-CA", "en-GB"],
      expectedQuickSuggestEnabled: true,
      queries: [
        {
          query: "amp",
          expectedResults: [
            QuickSuggestTestUtils.ampResult(),
            makeExpectedExposureResult("bbb"),
            makeExpectedExposureResult("aaa"),
          ],
        },
        {
          query: "wikipedia",
          expectedResults: [
            QuickSuggestTestUtils.wikipediaResult(),
            makeExpectedExposureResult("bbb"),
            makeExpectedExposureResult("aaa"),
          ],
        },
        {
          query: "aaa keyword",
          expectedResults: [makeExpectedExposureResult("aaa")],
        },
        {
          query: "aaa bbb keyword",
          expectedResults: [
            makeExpectedExposureResult("bbb"),
            makeExpectedExposureResult("aaa"),
          ],
        },
      ],
    },
  ];

  for (let test of tests) {
    await doLocaleTest(test);
  }
});

add_task(async function suggestDisabledLocales() {
  let queries = [
    {
      query: "amp",
      expectedResults: [
        // No AMP result!
        makeExpectedExposureResult("bbb"),
        makeExpectedExposureResult("aaa"),
      ],
    },
    {
      query: "wikipedia",
      expectedResults: [
        // No Wikipedia result!
        makeExpectedExposureResult("bbb"),
        makeExpectedExposureResult("aaa"),
      ],
    },
    {
      query: "aaa keyword",
      expectedResults: [makeExpectedExposureResult("aaa")],
    },
    {
      query: "aaa bbb keyword",
      expectedResults: [
        makeExpectedExposureResult("bbb"),
        makeExpectedExposureResult("aaa"),
      ],
    },
  ];

  let tests = [
    {
      homeRegion: "US",
      locales: ["de", "fr", "ja"],
      expectedQuickSuggestEnabled: false,
      queries,
    },
    {
      homeRegion: "CA",
      locales: ["en-US", "en-CA", "en-GB", "fr"],
      expectedQuickSuggestEnabled: false,
      queries,
    },
    {
      homeRegion: "DE",
      locales: ["de", "en-US", "fr"],
      expectedQuickSuggestEnabled: false,
      queries,
    },
  ];

  for (let test of tests) {
    await doLocaleTest(test);
  }
});

async function doLocaleTest({
  homeRegion,
  locales,
  expectedQuickSuggestEnabled,
  queries,
}) {
  for (let locale of locales) {
    info("Doing locale test: " + JSON.stringify({ homeRegion, locale }));

    // Set the region and locale.
    await QuickSuggestTestUtils.withLocales({
      homeRegion,
      locales: [locale],
      callback: async () => {
        // Reinitialize Suggest, which will set default-branch values for
        // Suggest prefs appropriate to the locale.
        info("Reinitializing Suggest");
        await QuickSuggest._test_reinit();
        info("Done reinitializing Suggest");

        // Sanity-check prefs. At this point, the value of `quickSuggestEnabled`
        // will be the value of its fallback pref, `quicksuggest.enabled`.
        assertSuggestPrefs(expectedQuickSuggestEnabled);
        Assert.equal(
          UrlbarPrefs.get("quickSuggestEnabled"),
          expectedQuickSuggestEnabled,
          "quickSuggestEnabled Nimbus variable should be correct after setting locale"
        );

        // Install an experiment that enables Suggest and exposures.
        let nimbusCleanup = await UrlbarTestUtils.initNimbusFeature({
          quickSuggestEnabled: true,
          quickSuggestExposureSuggestionTypes: "aaa,bbb",
        });
        await QuickSuggestTestUtils.forceSync();

        // All default- and user-branch Suggest prefs should remain the same.
        assertSuggestPrefs(expectedQuickSuggestEnabled);

        // But `quickSuggestEnabled` should be true, since we just installed
        // an experiment with it set to true.
        Assert.ok(
          UrlbarPrefs.get("quickSuggestEnabled"),
          "quickSuggestEnabled Nimbus variable should be enabled after installing experiment"
        );

        // Do a search and check the results.
        for (let { query, expectedResults } of queries) {
          await check_results({
            context: createContext(query, {
              providers: [UrlbarProviderQuickSuggest.name],
              isPrivate: false,
            }),
            matches: expectedResults,
          });
        }

        await nimbusCleanup();
        await QuickSuggestTestUtils.forceSync();
      },
    });
  }

  // Reinitialize Suggest so prefs go back to their defaults now that the app is
  // back to its default locale.
  await QuickSuggest._test_reinit();
}

function assertSuggestPrefs(expectedEnabled) {
  let prefs = [
    "browser.urlbar.quicksuggest.enabled",
    "browser.urlbar.suggest.quicksuggest.sponsored",
    "browser.urlbar.suggest.quicksuggest.nonsponsored",
  ];
  for (let p of prefs) {
    Assert.equal(
      Services.prefs.getDefaultBranch("").getBoolPref(p),
      expectedEnabled,
      "Default-branch value should be correct: " + p
    );
    Assert.equal(
      Services.prefs.getBranch("").getBoolPref(p),
      expectedEnabled,
      "User-branch value should be correct: " + p
    );
  }
}

function makeExpectedExposureResult(exposureSuggestionType) {
  return {
    type: UrlbarUtils.RESULT_TYPE.DYNAMIC,
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    heuristic: false,
    exposureTelemetry: UrlbarUtils.EXPOSURE_TELEMETRY.HIDDEN,
    payload: {
      exposureSuggestionType,
      source: "rust",
      dynamicType: "exposure",
      provider: "Exposure",
      telemetryType: "exposure",
      isSponsored: false,
    },
  };
}
