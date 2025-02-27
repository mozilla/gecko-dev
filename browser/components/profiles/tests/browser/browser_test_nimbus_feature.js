/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ExperimentFakes: "resource://testing-common/NimbusTestUtils.sys.mjs",
});

const PROFILES_PREF_NAME = "browser.profiles.enabled";

// Enabling the Nimbus feature should turn on the pref
add_task(async function test_nimbus_feature_enable() {
  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
  });

  await initGroupDatabase();

  info("Set profiles.enabled pref to false");
  await SpecialPowers.pushPrefEnv({
    set: [[PROFILES_PREF_NAME, false]],
  });

  info("Enrolling in selectableProfiles experiment");
  let experimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "selectableProfiles",
    value: {
      enabled: true,
    },
  });

  info("Check profiles.enabled pref is true");
  Assert.equal(
    Services.prefs.getBoolPref(PROFILES_PREF_NAME),
    true,
    "Experiment enabled set the pref value to `true`"
  );

  info("Remove experiment");
  experimentCleanup();

  info("Verify profiles.enabled pref is still true after experiment cleanup");
  Assert.equal(
    Services.prefs.getBoolPref(PROFILES_PREF_NAME),
    true,
    "Experiment removed didn't change pref value"
  );
});

// Disabling the Nimbus feature shouldn't turn off the pref
add_task(async function test_nimbus_feature_disable() {
  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
  });

  await initGroupDatabase();

  info("Set profiles.enabled pref to true");
  await SpecialPowers.pushPrefEnv({
    set: [[PROFILES_PREF_NAME, true]],
  });

  info("Disable experiment");
  let experimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "selectableProfiles",
    value: {
      enabled: false,
    },
  });

  info("Check profiles.enabled pref is true");
  Assert.equal(
    Services.prefs.getBoolPref(PROFILES_PREF_NAME),
    true,
    "Experiment disabled didn't change pref value"
  );

  info("Remove experiment");
  experimentCleanup();

  info("Verify profiles.enabled pref is still true after experiment cleanup");
  Assert.equal(
    Services.prefs.getBoolPref(PROFILES_PREF_NAME),
    true,
    "Experiment removal didn't change pref value"
  );
});
