/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  MODE_DISABLED,
  MODE_ENABLED,
  MODE_ENABLED_STANDBY,
  MODE_ENABLED_DRY_RUN,
} = Ci.nsIBounceTrackingProtection;

const BTP_MODE_PREF = "privacy.bounceTrackingProtection.mode";

/**
 * Run a bounce test with a custom bounce tracking protection mode.
 * @param {Number} mode - Mode to set for BTP. Any of
 * Ci.nsIBounceTrackingProtection.MODE_*
 * @param {boolean} shouldBeEnabled - Whether BTP should classify + purge in
 * this mode.
 */
async function runTestModePref(mode, shouldBeEnabled) {
  info("runTestModePref " + JSON.stringify({ mode, shouldBeEnabled }));
  await SpecialPowers.pushPrefEnv({
    set: [[BTP_MODE_PREF, mode]],
  });

  info("Run server bounce with cookie.");
  await runTestBounce({
    bounceType: "server",
    setState: "cookie-server",
    expectRecordBounces: shouldBeEnabled,
    expectCandidate: shouldBeEnabled,
    expectPurge: shouldBeEnabled,
  });

  await SpecialPowers.popPrefEnv();
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.bounceTrackingProtection.requireStatefulBounces", true],
      ["privacy.bounceTrackingProtection.bounceTrackingGracePeriodSec", 0],
    ],
  });
});

/**
 * Tests classification + purging with different BTP modes.
 */
add_task(async function test_mode_pref() {
  await runTestModePref(MODE_DISABLED, false);
  await runTestModePref(MODE_ENABLED, true);
  await runTestModePref(MODE_ENABLED_STANDBY, false);
  await runTestModePref(MODE_ENABLED_DRY_RUN, true);
});

/**
 * Tests that when the BTP mode is switched the bounce tracker candidate list is
 * cleared.
 */
add_task(async function test_mode_switch_clears_bounce_candidates() {
  // Start with MODE_ENABLED
  let modeOriginal = Services.prefs.getIntPref(BTP_MODE_PREF);
  registerCleanupFunction(() => {
    Services.prefs.setIntPref(BTP_MODE_PREF, modeOriginal);
    bounceTrackingProtection.clearAll();
  });

  info(
    "Populate BTP state: Add bounce tracker candidates and a user activation."
  );
  bounceTrackingProtection.testAddBounceTrackerCandidate(
    {},
    "bounce-tracker.com",
    1
  );
  bounceTrackingProtection.testAddBounceTrackerCandidate(
    {},
    "another-bounce-tracker.net",
    2
  );
  bounceTrackingProtection.testAddUserActivation(
    {},
    "user-activation.com",
    400
  );
  Assert.equal(
    bounceTrackingProtection.testGetBounceTrackerCandidateHosts({}).length,
    2,
    "Bounce tracker candidate map should have been populated."
  );
  Assert.equal(
    bounceTrackingProtection.testGetUserActivationHosts({}).length,
    1,
    "User activation map should have been populated."
  );

  info("Update to MODE_ENABLED_DRY_RUN");
  Services.prefs.setIntPref(BTP_MODE_PREF, MODE_ENABLED_DRY_RUN);

  Assert.equal(
    bounceTrackingProtection.testGetBounceTrackerCandidateHosts({}).length,
    0,
    "Mode change should have cleared bouncer tracker candidate map."
  );
  Assert.equal(
    bounceTrackingProtection.testGetUserActivationHosts({}).length,
    1,
    "Mode change should NOT have cleared user activation map."
  );

  info("Add a new bounce tracker");
  bounceTrackingProtection.testAddBounceTrackerCandidate(
    {},
    "bounce-tracker2.com",
    1
  );
  Assert.equal(
    bounceTrackingProtection.testGetBounceTrackerCandidateHosts({}).length,
    1,
    "Bounce tracker candidate map should have been populated."
  );

  info("Switch back to MODE_ENABLED");
  Services.prefs.setIntPref(BTP_MODE_PREF, MODE_ENABLED);

  Assert.equal(
    bounceTrackingProtection.testGetBounceTrackerCandidateHosts({}).length,
    0,
    "Mode change should have cleared bouncer tracker candidate map."
  );
  Assert.equal(
    bounceTrackingProtection.testGetUserActivationHosts({}).length,
    1,
    "Mode change should NOT have cleared user activation map."
  );
});
