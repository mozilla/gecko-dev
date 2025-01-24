/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);

add_setup(async function () {
  let defaultPrefBranch = Services.prefs.getDefaultBranch("");
  let originalBTPMode = defaultPrefBranch.getIntPref(
    "privacy.bounceTrackingProtection.mode"
  );

  registerCleanupFunction(() => {
    defaultPrefBranch.setIntPref(
      "privacy.bounceTrackingProtection.mode",
      originalBTPMode
    );
    Services.fog.testResetFOG();
  });

  // Start with STANDBY mode. We need to set the default pref because Nimbus won't override user prefs.
  Services.prefs.clearUserPref("privacy.bounceTrackingProtection.mode");
  defaultPrefBranch.setIntPref(
    "privacy.bounceTrackingProtection.mode",
    Ci.nsIBounceTrackingProtection.MODE_ENABLED_STANDBY
  );

  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.bounceTrackingProtection.requireStatefulBounces", true],
      ["privacy.bounceTrackingProtection.bounceTrackingGracePeriodSec", 0],
    ],
  });
});

add_task(async function test_nimbus_exposure() {
  Services.fog.testResetFOG();

  Assert.equal(
    Services.prefs.getIntPref("privacy.bounceTrackingProtection.mode"),
    Ci.nsIBounceTrackingProtection.MODE_ENABLED_STANDBY,
    "BTP should be in initial mode MODE_ENABLED_STANDBY."
  );

  info("Enroll into the experiment which enables BTP.");
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "bounceTrackingProtection",
    value: {
      mode: Ci.nsIBounceTrackingProtection.MODE_ENABLED,
    },
  });

  Assert.equal(
    Services.prefs.getIntPref("privacy.bounceTrackingProtection.mode"),
    Ci.nsIBounceTrackingProtection.MODE_ENABLED,
    "After enrollment BTP should be in mode MODE_ENABLED."
  );

  let exposureEvents = Glean.normandy.exposeNimbusExperiment.testGetValue();
  Assert.equal(
    undefined,
    exposureEvents,
    "No Glean exposure events before BTP purge."
  );

  info("Test client bounce with cookie.");
  await runTestBounce({
    bounceType: "client",
    setState: "cookie-client",
    postBounceCallback: () => {
      exposureEvents = Glean.normandy.exposeNimbusExperiment.testGetValue();
      Assert.equal(
        undefined,
        exposureEvents,
        "No Glean exposure events after classification, but before BTP purge."
      );
    },
  });

  exposureEvents = Glean.normandy.exposeNimbusExperiment.testGetValue();
  Assert.equal(
    1,
    exposureEvents?.length,
    "There should be one exposure event after BTP purged."
  );
  Assert.equal(
    "bounceTrackingProtection",
    exposureEvents[0].extra.featureId,
    "Feature ID matches BTP."
  );

  await doExperimentCleanup();
  Services.fog.testResetFOG();
});

add_task(async function test_nimbus_no_exposure_dry_run() {
  Services.fog.testResetFOG();

  Assert.equal(
    Services.prefs.getIntPref("privacy.bounceTrackingProtection.mode"),
    Ci.nsIBounceTrackingProtection.MODE_ENABLED_STANDBY,
    "BTP should be in initial mode MODE_ENABLED_STANDBY."
  );

  info("Enroll into the experiment which enables BTP in MODE_ENABLED_DRY_RUN.");
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "bounceTrackingProtection",
    value: {
      mode: Ci.nsIBounceTrackingProtection.MODE_ENABLED_DRY_RUN,
    },
  });

  Assert.equal(
    Services.prefs.getIntPref("privacy.bounceTrackingProtection.mode"),
    Ci.nsIBounceTrackingProtection.MODE_ENABLED_DRY_RUN,
    "After enrollment BTP should be in mode MODE_ENABLED_DRY_RUN."
  );

  let exposureEvents = Glean.normandy.exposeNimbusExperiment.testGetValue();
  Assert.equal(
    undefined,
    exposureEvents,
    "No Glean exposure events before BTP purge."
  );

  info("Test client bounce with cookie.");
  await runTestBounce({
    bounceType: "client",
    setState: "cookie-client",
  });

  exposureEvents = Glean.normandy.exposeNimbusExperiment.testGetValue();
  Assert.equal(
    undefined,
    exposureEvents,
    "No Glean exposure events after BTP dry-run purge."
  );

  await doExperimentCleanup();
  Services.fog.testResetFOG();
});
