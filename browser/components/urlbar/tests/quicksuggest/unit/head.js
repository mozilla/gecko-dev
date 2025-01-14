/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../../unit/head.js */
/* eslint-disable jsdoc/require-param */

ChromeUtils.defineESModuleGetters(this, {
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.sys.mjs",
  UrlbarProviderAutofill: "resource:///modules/UrlbarProviderAutofill.sys.mjs",
  UrlbarProviderQuickSuggest:
    "resource:///modules/UrlbarProviderQuickSuggest.sys.mjs",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.sys.mjs",
});

add_setup(async function setUpQuickSuggestXpcshellTest() {
  // Initializing TelemetryEnvironment in an xpcshell environment requires
  // jumping through a bunch of hoops. Suggest's use of TelemetryEnvironment is
  // tested in browser tests, and there's no other necessary reason to wait for
  // TelemetryEnvironment initialization in xpcshell tests, so just skip it.
  UrlbarPrefs._testSkipTelemetryEnvironmentInit = true;
});

/**
 * Tests quick suggest prefs migrations.
 *
 * @param {object} options
 *   The options object.
 * @param {object} options.testOverrides
 *   An object that modifies how migration is performed. It has the following
 *   properties, and all are optional:
 *
 *   {number} migrationVersion
 *     Migration will stop at this version, so for example you can test
 *     migration only up to version 1 even when the current actual version is
 *     larger than 1.
 *   {object} defaultPrefs
 *     An object that maps pref names (relative to `browser.urlbar`) to
 *     default-branch values. These should be the default prefs for the given
 *     `migrationVersion` and will be set as defaults before migration occurs.
 *
 * @param {string} options.scenario
 *   The scenario to set at the time migration occurs.
 * @param {object} options.expectedPrefs
 *   The expected prefs after migration: `{ defaultBranch, userBranch }`
 *   Pref names should be relative to `browser.urlbar`.
 * @param {object} [options.initialUserBranch]
 *   Prefs to set on the user branch before migration ocurs. Use these to
 *   simulate user actions like disabling prefs or opting in or out of the
 *   online modal. Pref names should be relative to `browser.urlbar`.
 */
async function doMigrateTest({
  testOverrides,
  scenario,
  expectedPrefs,
  initialUserBranch = {},
}) {
  info(
    "Testing migration: " +
      JSON.stringify({
        testOverrides,
        initialUserBranch,
        scenario,
        expectedPrefs,
      })
  );

  function setPref(branch, name, value) {
    switch (typeof value) {
      case "boolean":
        branch.setBoolPref(name, value);
        break;
      case "number":
        branch.setIntPref(name, value);
        break;
      case "string":
        branch.setCharPref(name, value);
        break;
      default:
        Assert.ok(
          false,
          `Pref type not handled for setPref: ${name} = ${value}`
        );
        break;
    }
  }

  function getPref(branch, name) {
    let type = typeof UrlbarPrefs.get(name);
    switch (type) {
      case "boolean":
        return branch.getBoolPref(name);
      case "number":
        return branch.getIntPref(name);
      case "string":
        return branch.getCharPref(name);
      default:
        Assert.ok(false, `Pref type not handled for getPref: ${name} ${type}`);
        break;
    }
    return null;
  }

  let defaultBranch = Services.prefs.getDefaultBranch("browser.urlbar.");
  let userBranch = Services.prefs.getBranch("browser.urlbar.");

  // Set initial prefs. `initialDefaultBranch` are firefox.js values, i.e.,
  // defaults immediately after startup and before any scenario update and
  // migration happens.
  UrlbarPrefs._updatingFirefoxSuggestScenario = true;
  UrlbarPrefs.clear("quicksuggest.migrationVersion");
  let initialDefaultBranch = {
    "suggest.quicksuggest.nonsponsored": false,
    "suggest.quicksuggest.sponsored": false,
    "quicksuggest.dataCollection.enabled": false,
  };
  for (let name of Object.keys(initialDefaultBranch)) {
    userBranch.clearUserPref(name);
  }
  for (let [branch, prefs] of [
    [defaultBranch, initialDefaultBranch],
    [userBranch, initialUserBranch],
  ]) {
    for (let [name, value] of Object.entries(prefs)) {
      if (value !== undefined) {
        setPref(branch, name, value);
      }
    }
  }
  UrlbarPrefs._updatingFirefoxSuggestScenario = false;

  // Update the scenario and check prefs twice. The first time the migration
  // should happen, and the second time the migration should not happen and
  // all the prefs should stay the same.
  for (let i = 0; i < 2; i++) {
    info(`Calling updateFirefoxSuggestScenario, i=${i}`);

    // Do the scenario update and set `isStartup` to simulate startup.
    await UrlbarPrefs.updateFirefoxSuggestScenario({
      ...testOverrides,
      scenario,
      isStartup: true,
    });

    // Check expected pref values. Store expected effective values as we go so
    // we can check them afterward. For a given pref, the expected effective
    // value is the user value, or if there's not a user value, the default
    // value.
    let expectedEffectivePrefs = {};
    let {
      defaultBranch: expectedDefaultBranch,
      userBranch: expectedUserBranch,
    } = expectedPrefs;
    expectedDefaultBranch = expectedDefaultBranch || {};
    expectedUserBranch = expectedUserBranch || {};
    for (let [branch, prefs, branchType] of [
      [defaultBranch, expectedDefaultBranch, "default"],
      [userBranch, expectedUserBranch, "user"],
    ]) {
      let entries = Object.entries(prefs);
      if (!entries.length) {
        continue;
      }

      info(
        `Checking expected prefs on ${branchType} branch after updating scenario`
      );
      for (let [name, value] of entries) {
        expectedEffectivePrefs[name] = value;
        if (branch == userBranch) {
          Assert.ok(
            userBranch.prefHasUserValue(name),
            `Pref ${name} is on user branch`
          );
        }
        Assert.equal(
          getPref(branch, name),
          value,
          `Pref ${name} value on ${branchType} branch`
        );
      }
    }

    info(
      `Making sure prefs on the default branch without expected user-branch values are not on the user branch`
    );
    for (let name of Object.keys(initialDefaultBranch)) {
      if (!expectedUserBranch.hasOwnProperty(name)) {
        Assert.ok(
          !userBranch.prefHasUserValue(name),
          `Pref ${name} is not on user branch`
        );
      }
    }

    info(`Checking expected effective prefs`);
    for (let [name, value] of Object.entries(expectedEffectivePrefs)) {
      Assert.equal(
        UrlbarPrefs.get(name),
        value,
        `Pref ${name} effective value`
      );
    }

    let currentVersion =
      testOverrides?.migrationVersion === undefined
        ? UrlbarPrefs.FIREFOX_SUGGEST_MIGRATION_VERSION
        : testOverrides.migrationVersion;
    Assert.equal(
      UrlbarPrefs.get("quicksuggest.migrationVersion"),
      currentVersion,
      "quicksuggest.migrationVersion is correct after migration"
    );
  }

  // Clean up.
  UrlbarPrefs._updatingFirefoxSuggestScenario = true;
  UrlbarPrefs.clear("quicksuggest.migrationVersion");
  let userBranchNames = [
    ...Object.keys(initialUserBranch),
    ...Object.keys(expectedPrefs.userBranch || {}),
  ];
  for (let name of userBranchNames) {
    userBranch.clearUserPref(name);
  }
  UrlbarPrefs._updatingFirefoxSuggestScenario = false;
}

/**
 * Does some "show less frequently" tests where the cap is set in remote
 * settings and Nimbus. See `doOneShowLessFrequentlyTest()`. This function
 * assumes the matching behavior implemented by the given `BaseFeature` is based
 * on matching prefixes of the given keyword starting at the first word. It
 * also assumes the `BaseFeature` provides suggestions in remote settings.
 *
 * @param {object} options
 *   Options object.
 * @param {BaseFeature} options.feature
 *   The feature that provides the suggestion matched by the searches.
 * @param {*} options.expectedResult
 *   The expected result that should be matched, for searches that are expected
 *   to match a result. Can also be a function; it's passed the current search
 *   string and it should return the expected result.
 * @param {string} options.showLessFrequentlyCountPref
 *   The name of the pref that stores the "show less frequently" count being
 *   tested.
 * @param {string} options.nimbusCapVariable
 *   The name of the Nimbus variable that stores the "show less frequently" cap
 *   being tested.
 * @param {object} options.keyword
 *   The primary keyword to use during the test.
 * @param {number} options.keywordBaseIndex
 *   The index in `keyword` to base substring checks around. This function will
 *   test substrings starting at the beginning of keyword and ending at the
 *   following indexes: one index before `keywordBaseIndex`,
 *   `keywordBaseIndex`, `keywordBaseIndex` + 1, `keywordBaseIndex` + 2, and
 *   `keywordBaseIndex` + 3.
 */
async function doShowLessFrequentlyTests({
  feature,
  expectedResult,
  showLessFrequentlyCountPref,
  nimbusCapVariable,
  keyword,
  keywordBaseIndex = keyword.indexOf(" "),
}) {
  // Do some sanity checks on the keyword. Any checks that fail are errors in
  // the test.
  if (keywordBaseIndex <= 0) {
    throw new Error(
      "keywordBaseIndex must be > 0, but it's " + keywordBaseIndex
    );
  }
  if (keyword.length < keywordBaseIndex + 3) {
    throw new Error(
      "keyword must have at least two chars after keywordBaseIndex"
    );
  }

  let tests = [
    {
      showLessFrequentlyCount: 0,
      canShowLessFrequently: true,
      newSearches: {
        [keyword.substring(0, keywordBaseIndex - 1)]: false,
        [keyword.substring(0, keywordBaseIndex)]: true,
        [keyword.substring(0, keywordBaseIndex + 1)]: true,
        [keyword.substring(0, keywordBaseIndex + 2)]: true,
        [keyword.substring(0, keywordBaseIndex + 3)]: true,
      },
    },
    {
      showLessFrequentlyCount: 1,
      canShowLessFrequently: true,
      newSearches: {
        [keyword.substring(0, keywordBaseIndex)]: false,
      },
    },
    {
      showLessFrequentlyCount: 2,
      canShowLessFrequently: true,
      newSearches: {
        [keyword.substring(0, keywordBaseIndex + 1)]: false,
      },
    },
    {
      showLessFrequentlyCount: 3,
      canShowLessFrequently: false,
      newSearches: {
        [keyword.substring(0, keywordBaseIndex + 2)]: false,
      },
    },
    {
      showLessFrequentlyCount: 3,
      canShowLessFrequently: false,
      newSearches: {},
    },
  ];

  info("Testing 'show less frequently' with cap in remote settings");
  await doOneShowLessFrequentlyTest({
    tests,
    feature,
    expectedResult,
    showLessFrequentlyCountPref,
    rs: {
      show_less_frequently_cap: 3,
    },
  });

  // Nimbus should override remote settings.
  info("Testing 'show less frequently' with cap in Nimbus and remote settings");
  await doOneShowLessFrequentlyTest({
    tests,
    feature,
    expectedResult,
    showLessFrequentlyCountPref,
    rs: {
      show_less_frequently_cap: 10,
    },
    nimbus: {
      [nimbusCapVariable]: 3,
    },
  });
}

/**
 * Does a group of searches, increments the "show less frequently" count, and
 * repeats until all groups are done. The cap can be set by remote settings
 * config and/or Nimbus.
 *
 * @param {object} options
 *   Options object.
 * @param {BaseFeature} options.feature
 *   The feature that provides the suggestion matched by the searches.
 * @param {*} options.expectedResult
 *   The expected result that should be matched, for searches that are expected
 *   to match a result. Can also be a function; it's passed the current search
 *   string and it should return the expected result.
 * @param {string} options.showLessFrequentlyCountPref
 *   The name of the pref that stores the "show less frequently" count being
 *   tested.
 * @param {object} options.tests
 *   An array where each item describes a group of new searches to perform and
 *   expected state. Each item should look like this:
 *   `{ showLessFrequentlyCount, canShowLessFrequently, newSearches }`
 *
 *   {number} showLessFrequentlyCount
 *     The expected value of `showLessFrequentlyCount` before the group of
 *     searches is performed.
 *   {boolean} canShowLessFrequently
 *     The expected value of `canShowLessFrequently` before the group of
 *     searches is performed.
 *   {object} newSearches
 *     An object that maps each search string to a boolean that indicates
 *     whether the first remote settings suggestion should be triggered by the
 *     search string. Searches are cumulative: The intended use is to pass a
 *     large initial group of searches in the first search group, and then each
 *     following `newSearches` is a diff against the previous.
 * @param {object} options.rs
 *   The remote settings config to set.
 * @param {object} options.nimbus
 *   The Nimbus variables to set.
 */
async function doOneShowLessFrequentlyTest({
  feature,
  expectedResult,
  showLessFrequentlyCountPref,
  tests,
  rs = {},
  nimbus = {},
}) {
  // Disable Merino so we trigger only remote settings suggestions. The
  // `BaseFeature` is expected to add remote settings suggestions using keywords
  // start starting with the first word in each full keyword, but the mock
  // Merino server will always return whatever suggestion it's told to return
  // regardless of the search string. That means Merino will return a suggestion
  // for a keyword that's smaller than the first full word.
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", false);

  // Set Nimbus variables and RS config.
  let cleanUpNimbus = await UrlbarTestUtils.initNimbusFeature(nimbus);
  await QuickSuggestTestUtils.withConfig({
    config: rs,
    callback: async () => {
      let cumulativeSearches = {};

      for (let {
        showLessFrequentlyCount,
        canShowLessFrequently,
        newSearches,
      } of tests) {
        info(
          "Starting subtest: " +
            JSON.stringify({
              showLessFrequentlyCount,
              canShowLessFrequently,
              newSearches,
            })
        );

        Assert.equal(
          feature.showLessFrequentlyCount,
          showLessFrequentlyCount,
          "showLessFrequentlyCount should be correct initially"
        );
        Assert.equal(
          UrlbarPrefs.get(showLessFrequentlyCountPref),
          showLessFrequentlyCount,
          "Pref should be correct initially"
        );
        Assert.equal(
          feature.canShowLessFrequently,
          canShowLessFrequently,
          "canShowLessFrequently should be correct initially"
        );

        // Merge the current `newSearches` object into the cumulative object.
        cumulativeSearches = {
          ...cumulativeSearches,
          ...newSearches,
        };

        for (let [searchString, isExpected] of Object.entries(
          cumulativeSearches
        )) {
          info("Doing search: " + JSON.stringify({ searchString, isExpected }));

          let results = [];
          if (isExpected) {
            results.push(
              typeof expectedResult == "function"
                ? expectedResult(searchString)
                : expectedResult
            );
          }

          await check_results({
            context: createContext(searchString, {
              providers: [UrlbarProviderQuickSuggest.name],
              isPrivate: false,
            }),
            matches: results,
          });
        }

        feature.incrementShowLessFrequentlyCount();
      }
    },
  });

  await cleanUpNimbus();
  UrlbarPrefs.clear(showLessFrequentlyCountPref);
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", true);
}

/**
 * Queries the Rust component directly and checks the returned suggestions. The
 * point is to make sure the Rust backend passes the correct providers to the
 * Rust component depending on the types of enabled suggestions. Assuming the
 * Rust component isn't buggy, it should return suggestions only for the
 * passed-in providers.
 *
 * @param {object} options
 *   Options object
 * @param {string} options.searchString
 *   The search string.
 * @param {Array} options.tests
 *   Array of test objects: `{ prefs, expectedUrls }`
 *
 *   For each object, the given prefs are set, the Rust component is queried
 *   using the given search string, and the URLs of the returned suggestions are
 *   compared to the given expected URLs (order doesn't matter).
 *
 *   {object} prefs
 *     An object mapping pref names (relative to `browser.urlbar`) to values.
 *     These prefs will be set before querying and should be used to enable or
 *     disable particular types of suggestions.
 *   {Array} expectedUrls
 *     An array of the URLs of the suggestions that are expected to be returned.
 *     The order doesn't matter.
 */
async function doRustProvidersTests({ searchString, tests }) {
  for (let { prefs, expectedUrls } of tests) {
    info(
      "Starting Rust providers test: " + JSON.stringify({ prefs, expectedUrls })
    );

    info("Setting prefs and forcing sync");
    for (let [name, value] of Object.entries(prefs)) {
      UrlbarPrefs.set(name, value);
    }
    await QuickSuggestTestUtils.forceSync();

    info("Querying with search string: " + JSON.stringify(searchString));
    let suggestions = await QuickSuggest.rustBackend.query(searchString);
    info("Got suggestions: " + JSON.stringify(suggestions));

    Assert.deepEqual(
      suggestions.map(s => s.url).sort(),
      expectedUrls.sort(),
      "query() should return the expected suggestions (by URL)"
    );

    info("Clearing prefs and forcing sync");
    for (let name of Object.keys(prefs)) {
      UrlbarPrefs.clear(name);
    }
    await QuickSuggestTestUtils.forceSync();
  }
}

/**
 * Simulates performing a command for a feature by calling its `onEngagement()`.
 *
 * @param {object} options
 *   Options object.
 * @param {SuggestFeature} options.feature
 *   The feature whose command will be triggered.
 * @param {string} options.command
 *   The name of the command to trigger.
 * @param {UrlbarResult} options.result
 *   The result that the command will act on.
 * @param {string} options.searchString
 *   The search string to pass to `onEngagement()`.
 */
function triggerCommand({ feature, command, result, searchString = "" }) {
  info(`Calling ${feature.name}.onEngagement() to trigger command: ${command}`);
  feature.onEngagement(
    // query context
    {},
    // controller
    {
      removeResult() {},
      view: {
        acknowledgeFeedback() {},
        invalidateResultMenuCommands() {},
      },
    },
    // details
    { result, selType: command },
    searchString
  );
}
