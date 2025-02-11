/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests quick suggest prefs migration to version 2.

"use strict";

// Expected version 2 default-branch prefs
const DEFAULT_PREFS = {
  history: {
    "quicksuggest.enabled": false,
  },
  offline: {
    "quicksuggest.enabled": true,
    "quicksuggest.dataCollection.enabled": false,
    "quicksuggest.shouldShowOnboardingDialog": false,
    "suggest.quicksuggest.nonsponsored": true,
    "suggest.quicksuggest.sponsored": true,
  },
};

// Migration will use these values to migrate only up to version 1 instead of
// the current version.
// Currently undefined because version 2 is the current migration version and we
// want migration to use its actual values, not overrides. When version 3 is
// added, set this to an object like the one in test_quicksuggest_migrate_v1.js.
const TEST_OVERRIDES = undefined;

add_setup(async () => {
  await UrlbarTestUtils.initNimbusFeature();
});

// The following tasks test OFFLINE UNVERSIONED to OFFLINE

// Migrating from:
// * Unversioned prefs
// * Offline
// * Main suggestions pref: user left on
// * Sponsored suggestions: user left on
//
// Scenario when migration occurs:
// * Offline
//
// Expected:
// * Non-sponsored suggestions: on
// * Sponsored suggestions: on
// * Data collection: off
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    scenario: "offline",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.offline,
    },
  });
});

// Migrating from:
// * Unversioned prefs
// * Offline
// * Main suggestions pref: user turned off
// * Sponsored suggestions: user left on (but ignored since main was off)
//
// Scenario when migration occurs:
// * Offline
//
// Expected:
// * Non-sponsored suggestions: off
// * Sponsored suggestions: off
// * Data collection: off
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "suggest.quicksuggest": false,
    },
    scenario: "offline",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": false,
      },
    },
  });
});

// Migrating from:
// * Unversioned prefs
// * Offline
// * Main suggestions pref: user left on
// * Sponsored suggestions: user turned off
//
// Scenario when migration occurs:
// * Offline
//
// Expected:
// * Non-sponsored suggestions: on
// * Sponsored suggestions: off
// * Data collection: off
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "suggest.quicksuggest.sponsored": false,
    },
    scenario: "offline",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest.sponsored": false,
      },
    },
  });
});

// Migrating from:
// * Unversioned prefs
// * Offline
// * Main suggestions pref: user turned off
// * Sponsored suggestions: user turned off
//
// Scenario when migration occurs:
// * Offline
//
// Expected:
// * Non-sponsored suggestions: off
// * Sponsored suggestions: off
// * Data collection: off
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "suggest.quicksuggest": false,
      "suggest.quicksuggest.sponsored": false,
    },
    scenario: "offline",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": false,
      },
    },
  });
});

// The following tasks test OFFLINE VERSION 1 to OFFLINE

// Migrating from:
// * Version 1 prefs
// * Offline
// * Non-sponsored suggestions: user left on
// * Sponsored suggestions: user left on
// * Data collection: user left off
//
// Scenario when migration occurs:
// * Offline
//
// Expected:
// * Non-sponsored suggestions: on
// * Sponsored suggestions: on
// * Data collection: off
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "quicksuggest.migrationVersion": 1,
      "quicksuggest.scenario": "offline",
    },
    scenario: "offline",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.offline,
    },
  });
});

// Migrating from:
// * Version 1 prefs
// * Offline
// * Non-sponsored suggestions: user left on
// * Sponsored suggestions: user left on
// * Data collection: user turned on
//
// Scenario when migration occurs:
// * Offline
//
// Expected:
// * Non-sponsored suggestions: on
// * Sponsored suggestions: on
// * Data collection: on
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "quicksuggest.migrationVersion": 1,
      "quicksuggest.scenario": "offline",
      "quicksuggest.dataCollection.enabled": true,
    },
    scenario: "offline",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.offline,
      userBranch: {
        "quicksuggest.dataCollection.enabled": true,
      },
    },
  });
});

// Migrating from:
// * Version 1 prefs
// * Offline
// * Non-sponsored suggestions: user left on
// * Sponsored suggestions: user turned off
// * Data collection: user left off
//
// Scenario when migration occurs:
// * Offline
//
// Expected:
// * Non-sponsored suggestions: on
// * Sponsored suggestions: off
// * Data collection: off
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "quicksuggest.migrationVersion": 1,
      "quicksuggest.scenario": "offline",
      "suggest.quicksuggest.sponsored": false,
    },
    scenario: "offline",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest.sponsored": false,
      },
    },
  });
});

// Migrating from:
// * Version 1 prefs
// * Offline
// * Non-sponsored suggestions: user turned off
// * Sponsored suggestions: user turned off
// * Data collection: user left off
//
// Scenario when migration occurs:
// * Offline
//
// Expected:
// * Non-sponsored suggestions: off
// * Sponsored suggestions: off
// * Data collection: off
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "quicksuggest.migrationVersion": 1,
      "quicksuggest.scenario": "offline",
      "suggest.quicksuggest.nonsponsored": false,
      "suggest.quicksuggest.sponsored": false,
    },
    scenario: "offline",
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": false,
      },
    },
  });
});
