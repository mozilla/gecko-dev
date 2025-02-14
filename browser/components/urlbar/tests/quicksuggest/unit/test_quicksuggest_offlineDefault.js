/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that Suggest is enabled by default for "en" locales in the U.S. and
// disabled by default everywhere else. (When Suggest is enabled by default,
// "offline" suggestions are enabled but Merino is not, hence "offline"
// default.)

"use strict";

// All the prefs that are set when Suggest is initialized along with the
// expected default-branch values.
const PREFS = [
  {
    name: "browser.urlbar.quicksuggest.enabled",
    get: "getBoolPref",
    set: "setBoolPref",
    expectedWhenSuggestEnabled: true,
    expectedWhenSuggestDisabled: false,
  },
  {
    name: "browser.urlbar.quicksuggest.dataCollection.enabled",
    get: "getBoolPref",
    set: "setBoolPref",
    expectedWhenSuggestEnabled: false,
    expectedWhenSuggestDisabled: false,
  },
  {
    name: "browser.urlbar.suggest.quicksuggest.nonsponsored",
    get: "getBoolPref",
    set: "setBoolPref",
    expectedWhenSuggestEnabled: true,
    expectedWhenSuggestDisabled: false,
  },
  {
    name: "browser.urlbar.suggest.quicksuggest.sponsored",
    get: "getBoolPref",
    set: "setBoolPref",
    expectedWhenSuggestEnabled: true,
    expectedWhenSuggestDisabled: false,
  },
];

add_setup(async () => {
  await UrlbarTestUtils.initNimbusFeature();
});

add_task(async function test() {
  let tests = [
    { locale: "en-US", home: "US", expectSuggestToBeEnabled: true },
    { locale: "en-US", home: "CA", expectSuggestToBeEnabled: false },
    { locale: "en-CA", home: "US", expectSuggestToBeEnabled: true },
    { locale: "en-CA", home: "CA", expectSuggestToBeEnabled: false },
    { locale: "en-GB", home: "US", expectSuggestToBeEnabled: true },
    { locale: "en-GB", home: "GB", expectSuggestToBeEnabled: false },
    { locale: "de", home: "US", expectSuggestToBeEnabled: false },
    { locale: "de", home: "DE", expectSuggestToBeEnabled: false },
  ];
  for (let { locale, home, expectSuggestToBeEnabled } of tests) {
    await doTest({ locale, home, expectSuggestToBeEnabled });
  }
});

/**
 * Sets the app's locale and region, reinitializes Suggest, and asserts that the
 * pref values are correct.
 *
 * @param {object} options
 *   Options object.
 * @param {string} options.locale
 *   The locale to simulate.
 * @param {string} options.home
 *   The "home" region to simulate.
 * @param {boolean} options.expectSuggestToBeEnabled
 *   Whether Suggest is expected to be enabled by default for the given locale
 *   and region.
 */
async function doTest({ locale, home, expectSuggestToBeEnabled }) {
  // Setup: Clear any user values and save original default-branch values.
  for (let pref of PREFS) {
    Services.prefs.clearUserPref(pref.name);
    pref.originalDefault = Services.prefs
      .getDefaultBranch(pref.name)
      [pref.get]("");
  }

  // Set the region and locale, call the function, check the pref values.
  await QuickSuggestTestUtils.withLocales({
    homeRegion: home,
    locales: [locale],
    callback: async () => {
      await QuickSuggest._test_reinit();
      for (let {
        name,
        get,
        expectedWhenSuggestEnabled,
        expectedWhenSuggestDisabled,
      } of PREFS) {
        let expectedValue = expectSuggestToBeEnabled
          ? expectedWhenSuggestEnabled
          : expectedWhenSuggestDisabled;

        // Check the default-branch value.
        Assert.strictEqual(
          Services.prefs.getDefaultBranch(name)[get](""),
          expectedValue,
          `Default pref value for ${name}, locale ${locale}, home ${home}`
        );

        // For good measure, also check the return value of `UrlbarPrefs.get`
        // since we use it everywhere. The value should be the same as the
        // default-branch value.
        UrlbarPrefs.get(
          name.replace("browser.urlbar.", ""),
          expectedValue,
          `UrlbarPrefs.get() value for ${name}, locale ${locale}, home ${home}`
        );
      }
    },
  });

  // Teardown: Restore original default-branch values for the next task.
  for (let { name, originalDefault, set } of PREFS) {
    if (originalDefault === undefined) {
      Services.prefs.deleteBranch(name);
    } else {
      Services.prefs.getDefaultBranch(name)[set]("", originalDefault);
    }
  }
}
