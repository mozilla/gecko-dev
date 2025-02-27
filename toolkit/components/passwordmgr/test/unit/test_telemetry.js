/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the statistics and other counters reported through telemetry.
 */

"use strict";

// Globals

const MS_PER_DAY = 24 * 60 * 60 * 1000;

// To prevent intermittent failures when the test is executed at a time that is
// very close to a day boundary, we make it deterministic by using a static
// reference date for all the time-based statistics.
const gReferenceTimeMs = new Date("2000-01-01T00:00:00").getTime();

// Returns a milliseconds value to use with nsILoginMetaInfo properties, falling
// approximately in the middle of the specified number of days before the
// reference time, where zero days indicates a time within the past 24 hours.
const daysBeforeMs = days => gReferenceTimeMs - (days + 0.5) * MS_PER_DAY;

/**
 * Contains metadata that will be attached to test logins in order to verify
 * that the statistics collection is working properly. Most properties of the
 * logins are initialized to the default test values already.
 *
 * If you update this data or any of the telemetry histograms it checks, you'll
 * probably need to update the expected statistics in the test below.
 */
const StatisticsTestData = [
  {
    timeLastUsed: daysBeforeMs(0),
  },
  {
    timeLastUsed: daysBeforeMs(1),
  },
  {
    timeLastUsed: daysBeforeMs(7),
    formActionOrigin: null,
    httpRealm: "The HTTP Realm",
  },
  {
    username: "",
    timeLastUsed: daysBeforeMs(7),
  },
  {
    username: "",
    timeLastUsed: daysBeforeMs(30),
  },
  {
    username: "",
    timeLastUsed: daysBeforeMs(31),
  },
  {
    timeLastUsed: daysBeforeMs(365),
  },
  {
    username: "",
    timeLastUsed: daysBeforeMs(366),
  },
  {
    // If the login was saved in the future, it is ignored for statistiscs.
    timeLastUsed: daysBeforeMs(-1),
  },
  {
    timeLastUsed: daysBeforeMs(1000),
  },
];

// Tests

/**
 * Enable FOG and prepare the test data.
 */
add_setup(async () => {
  // FOG needs a profile directory to put its data in.
  do_get_profile();
  // FOG needs to be initialized, or testGetValue() calls will deadlock.
  Services.fog.initializeFOG();

  let uniqueNumber = 1;
  let logins = [];
  for (let loginModifications of StatisticsTestData) {
    loginModifications.origin = `http://${uniqueNumber++}.example.com`;
    if (typeof loginModifications.httpRealm != "undefined") {
      logins.push(TestData.authLogin(loginModifications));
    } else {
      logins.push(TestData.formLogin(loginModifications));
    }
  }
  await Services.logins.addLogins(logins);
});

/*
 * Tests that the number of saved logins is appropriately reported.
 */
add_task(function test_logins_count() {
  Assert.equal(
    Glean.pwmgr.numSavedPasswords.testGetValue(),
    StatisticsTestData.length,
    "We've appropriately counted all the logins"
  );
});

/**
 * Tests the collection of statistics related to general settings.
 */
add_task(function test_settings_statistics() {
  let oldRememberSignons = Services.prefs.getBoolPref("signon.rememberSignons");
  registerCleanupFunction(function () {
    Services.prefs.setBoolPref("signon.rememberSignons", oldRememberSignons);
  });

  for (let remember of [false, true]) {
    // This change should be observed immediately by the login service.
    Services.prefs.setBoolPref("signon.rememberSignons", remember);
    Assert.equal(
      Glean.pwmgr.savingEnabled.testGetValue(),
      remember,
      "The pref is correctly recorded."
    );
  }
});
