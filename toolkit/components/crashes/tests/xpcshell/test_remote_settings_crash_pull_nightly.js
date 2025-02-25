/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

const { RemoteSettingsCrashPull } = ChromeUtils.importESModule(
  "resource://gre/modules/RemoteSettingsCrashPull.sys.mjs"
);

add_setup(async function setup_test() {
  do_get_profile();
});

add_task(function test_pref_value() {
  const prefValue = Services.prefs.getBoolPref(
    "browser.crashReports.crashPull"
  );
  if (AppConstants.NIGHTLY_BUILD) {
    Assert.ok(prefValue, "RemoteSettingsCrashPull pref enabled on nightly");
  } else {
    Assert.ok(!prefValue, "RemoteSettingsCrashPull pref disabled on nightly");
  }
});

add_task(function test_pref_disabled() {
  const originalPref = Services.prefs.getBoolPref(
    "browser.crashReports.crashPull"
  );
  Services.prefs.setBoolPref("browser.crashReports.crashPull", false);

  const originalCollection = RemoteSettingsCrashPull.collection;
  let collectionCalled = false;
  RemoteSettingsCrashPull.collection = function () {
    console.debug("Calling collection()");
    collectionCalled = true;
  };

  RemoteSettingsCrashPull.start();

  Assert.ok(RemoteSettingsCrashPull, "RemoteSettingsCrashPull obtained.");
  Assert.ok(
    !collectionCalled,
    "Method collection() should not have been called"
  );

  RemoteSettingsCrashPull.collection = originalCollection;
  Services.prefs.setBoolPref("browser.crashReports.crashPull", originalPref);
});

add_task(function test_pref_enabled() {
  const originalPref = Services.prefs.getBoolPref(
    "browser.crashReports.crashPull"
  );
  Services.prefs.setBoolPref("browser.crashReports.crashPull", true);

  const originalCollection = RemoteSettingsCrashPull.collection;
  let collectionCalled = false;
  RemoteSettingsCrashPull.collection = function () {
    console.debug("Calling collection()");
    collectionCalled = true;
  };

  RemoteSettingsCrashPull.start();

  Assert.ok(RemoteSettingsCrashPull, "RemoteSettingsCrashPull obtained.");
  Assert.ok(collectionCalled, "Method collection() should have been called");

  RemoteSettingsCrashPull.collection = originalCollection;
  Services.prefs.setBoolPref("browser.crashReports.crashPull", originalPref);
});
