/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.defineESModuleGetters(this, {
  UrlbarProviderQuickSuggest:
    "resource:///modules/UrlbarProviderQuickSuggest.sys.mjs",
});

// Tests that registering an exposureResults pref and triggering a match causes
// the exposure event to be recorded on the UrlbarResults.
const REMOTE_SETTINGS_RESULTS = [
  QuickSuggestTestUtils.ampRemoteSettings(),
  QuickSuggestTestUtils.wikipediaRemoteSettings(),
];

add_setup(async function test_setup() {
  // FOG needs a profile directory to put its data in.
  do_get_profile();

  // FOG needs to be initialized in order for data to flow.
  Services.fog.initializeFOG();

  // Set up the remote settings client with the test data.
  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    remoteSettingsRecords: [
      {
        type: "data",
        attachment: REMOTE_SETTINGS_RESULTS,
      },
    ],
    prefs: [
      ["suggest.quicksuggest.nonsponsored", true],
      ["suggest.quicksuggest.sponsored", true],
    ],
  });
});

add_task(async function testExposureCheck() {
  UrlbarPrefs.set("exposureResults", suggestResultType("adm_sponsored"));
  UrlbarPrefs.set("showExposureResults", true);

  let context = createContext("amp", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });

  await check_results({
    context,
    matches: [QuickSuggestTestUtils.ampResult()],
  });

  Assert.equal(
    context.results[0].exposureResultType,
    suggestResultType("adm_sponsored")
  );
  Assert.equal(context.results[0].exposureResultHidden, false);
});

add_task(async function testExposureCheckMultiple() {
  UrlbarPrefs.set(
    "exposureResults",
    [
      suggestResultType("adm_sponsored"),
      suggestResultType("adm_nonsponsored"),
    ].join(",")
  );
  UrlbarPrefs.set("showExposureResults", true);

  let context = createContext("amp", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });

  await check_results({
    context,
    matches: [QuickSuggestTestUtils.ampResult()],
  });

  Assert.equal(
    context.results[0].exposureResultType,
    suggestResultType("adm_sponsored")
  );
  Assert.equal(context.results[0].exposureResultHidden, false);

  context = createContext("wikipedia", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });

  await check_results({
    context,
    matches: [QuickSuggestTestUtils.wikipediaResult()],
  });

  Assert.equal(
    context.results[0].exposureResultType,
    suggestResultType("adm_nonsponsored")
  );
  Assert.equal(context.results[0].exposureResultHidden, false);
});

add_task(async function exposureDisplayFiltering() {
  UrlbarPrefs.set("exposureResults", suggestResultType("adm_sponsored"));
  UrlbarPrefs.set("showExposureResults", false);

  let context = createContext("amp", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });

  await check_results({
    context,
    matches: [QuickSuggestTestUtils.ampResult()],
  });

  Assert.equal(
    context.results[0].exposureResultType,
    suggestResultType("adm_sponsored")
  );
  Assert.equal(context.results[0].exposureResultHidden, true);
});

function suggestResultType(typeWithoutSource) {
  let source = UrlbarPrefs.get("quickSuggestRustEnabled") ? "rust" : "rs";
  return `${source}_${typeWithoutSource}`;
}
