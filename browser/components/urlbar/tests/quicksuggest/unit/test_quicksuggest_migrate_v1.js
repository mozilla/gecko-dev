/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests quick suggest prefs migration from unversioned prefs to version 1.

"use strict";

// Expected version 1 default-branch prefs
const DEFAULT_PREFS = {
  "quicksuggest.enabled": true,
  "quicksuggest.dataCollection.enabled": false,
  "suggest.quicksuggest.nonsponsored": true,
  "suggest.quicksuggest.sponsored": true,
};

// Migration will use these values to migrate only up to version 1 instead of
// the current version.
const TEST_OVERRIDES = {
  migrationVersion: 1,
  defaultPrefs: DEFAULT_PREFS,
};

add_setup(async () => {
  await UrlbarTestUtils.initNimbusFeature();
});

// The following tasks test OFFLINE to version 1 when SUGGEST IS ENABLED

// Migrating from:
// * Offline (Suggest enabled by default)
// * User did not override any defaults
//
// Suggest enabled when migration occurs:
// * Yes
//
// Expected:
// * Non-sponsored suggestions: remain on
// * Sponsored suggestions: remain on
// * Data collection: off
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    shouldEnable: true,
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS,
    },
  });
});

// Migrating from:
// * Offline (Suggest enabled by default)
// * Main suggestions pref: user left on
// * Sponsored suggestions: user turned off
//
// Suggest enabled when migration occurs:
// * Yes
//
// Expected:
// * Non-sponsored suggestions: on
// * Sponsored suggestions: remain off
// * Data collection: off
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "suggest.quicksuggest.sponsored": false,
    },
    shouldEnable: true,
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS,
      userBranch: {
        "suggest.quicksuggest.sponsored": false,
      },
    },
  });
});

// Migrating from:
// * Offline (Suggest enabled by default)
// * Main suggestions pref: user turned off
// * Sponsored suggestions: user left on (but ignored since main was off)
//
// Suggest enabled when migration occurs:
// * Yes
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
    shouldEnable: true,
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": false,
      },
    },
  });
});

// Migrating from:
// * Offline (Suggest enabled by default)
// * Main suggestions pref: user turned off
// * Sponsored suggestions: user turned off
//
// Suggest enabled when migration occurs:
// * Yes
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
    shouldEnable: true,
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": false,
      },
    },
  });
});

// The following tasks test ONLINE to version 1 when SUGGEST IS ENABLED

// Migrating from:
// * Online (Suggest enabled but suggestions off by default)
// * User did not override any defaults
//
// Suggest enabled when migration occurs:
// * Yes
//
// Expected:
// * Non-sponsored suggestions: on (since main pref had default value)
// * Sponsored suggestions: on (since main & sponsored prefs had default values)
// * Data collection: off
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    shouldEnable: true,
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS,
    },
  });
});

// Migrating from:
// * Online (Suggest enabled but suggestions off by default)
// * Main suggestions pref: user left off
// * Sponsored suggestions: user turned on (but ignored since main was off)
//
// Suggest enabled when migration occurs:
// * Yes
//
// Expected:
// * Non-sponsored suggestions: off
// * Sponsored suggestions: on (see below)
// * Data collection: off
//
// It's unfortunate that sponsored suggestions are ultimately on since before
// the migration no suggestions were shown to the user. There's nothing we can
// do about it, aside from forcing off suggestions in more cases than we want.
// The reason is that at the time of migration we can't tell that the previous
// scenario was online -- or more precisely that it wasn't history. If we knew
// it wasn't history, then we'd know to turn sponsored off; if we knew it *was*
// history, then we'd know to turn sponsored -- and non-sponsored -- on, since
// the scenario at the time of migration is offline, where suggestions should be
// enabled by default.
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "suggest.quicksuggest.sponsored": true,
    },
    shouldEnable: true,
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS,
      userBranch: {
        "suggest.quicksuggest.sponsored": true,
      },
    },
  });
});

// Migrating from:
// * Online (Suggest enabled but suggestions off by default)
// * Main suggestions pref: user turned on
// * Sponsored suggestions: user left off
//
// Suggest enabled when migration occurs:
// * Yes
//
// Expected:
// * Non-sponsored suggestions: remain on
// * Sponsored suggestions: remain off
// * Data collection: off
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "suggest.quicksuggest": true,
    },
    shouldEnable: true,
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": true,
      },
    },
  });
});

// Migrating from:
// * Online (Suggest enabled but suggestions off by default)
// * Main suggestions pref: user turned on
// * Sponsored suggestions: user turned on
//
// Suggest enabled when migration occurs:
// * Yes
//
// Expected:
// * Non-sponsored suggestions: remain on
// * Sponsored suggestions: remain on
// * Data collection: off
add_task(async function () {
  await doMigrateTest({
    testOverrides: TEST_OVERRIDES,
    initialUserBranch: {
      "suggest.quicksuggest": true,
      "suggest.quicksuggest.sponsored": true,
    },
    shouldEnable: true,
    expectedPrefs: {
      defaultBranch: DEFAULT_PREFS,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": true,
        "suggest.quicksuggest.sponsored": true,
      },
    },
  });
});
