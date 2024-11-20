/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentAPI } = ChromeUtils.importESModule(
  "resource://nimbus/ExperimentAPI.sys.mjs"
);

const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);

add_task(async function test_nimbus_tracker_cookie_blocking_feature() {
  await ExperimentAPI.ready();

  let originalPrefValue = Services.prefs.getBoolPref(
    "network.cookie.cookieBehavior.trackerCookieBlocking"
  );

  info("Enroll with the third party tracker cookie blocking feature.");
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "thirdPartyTrackerCookieBlocking",
    value: {
      enabled: true,
    },
  });

  is(
    Services.prefs.getBoolPref(
      "network.cookie.cookieBehavior.trackerCookieBlocking"
    ),
    true,
    "The third party tracker cookie blocking pref has been set correctly"
  );
  is(
    Services.prefs
      .getDefaultBranch("")
      .getBoolPref("network.cookie.cookieBehavior.trackerCookieBlocking"),
    true,
    "The third party tracker cookie blocking pref has been set correctly to the default branch"
  );

  doExperimentCleanup();

  is(
    Services.prefs.getBoolPref(
      "network.cookie.cookieBehavior.trackerCookieBlocking"
    ),
    originalPrefValue,
    "The third party tracker cookie blocking pref has been reset correctly"
  );

  info(
    "Enroll with the third party tracker cookie blocking feature with different settings."
  );

  doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "thirdPartyTrackerCookieBlocking",
    value: {
      enabled: false,
    },
  });

  is(
    Services.prefs.getBoolPref(
      "network.cookie.cookieBehavior.trackerCookieBlocking"
    ),
    false,
    "The third party tracker cookie blocking pref has been set correctly"
  );
  is(
    Services.prefs
      .getDefaultBranch("")
      .getBoolPref("network.cookie.cookieBehavior.trackerCookieBlocking"),
    false,
    "The third party tracker cookie blocking pref has been set correctly to the default branch"
  );

  doExperimentCleanup();

  is(
    Services.prefs.getBoolPref(
      "network.cookie.cookieBehavior.trackerCookieBlocking"
    ),
    originalPrefValue,
    "The third party tracker cookie blocking pref has been reset correctly"
  );
});
