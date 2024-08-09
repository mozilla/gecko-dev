/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let btp;

add_setup(function () {
  // Need a profile to data clearing calls.
  do_get_profile();

  btp = Cc["@mozilla.org/bounce-tracking-protection;1"].getService(
    Ci.nsIBounceTrackingProtection
  );

  // Reset global bounce tracking state.
  btp.clearAll();

  // Clear telemetry before test.
  Services.fog.testResetFOG();
});

/**
 * Test which classfies `num` bounce trackers, runs one purge and checks
 * whether the correct num_hosts_per_purge_run telemetry is collected.
 *
 * @param {bool} isDryRunMode - Whether to enable btp in dry mode or normal mode
 * @param {bool} num - Number of hosts to purge
 */
async function testNumHostsPerPurgeRun(isDryRunMode, num) {
  Services.prefs.setBoolPref(
    "privacy.bounceTrackingProtection.enableDryRunMode",
    isDryRunMode
  );

  Assert.equal(
    Glean.bounceTrackingProtection.numHostsPerPurgeRun.testGetValue(),
    null,
    "Telemetry should be empty at the beginning"
  );

  for (let i = 0; i < num; i++) {
    btp.testAddBounceTrackerCandidate({}, `${i}.com`, 1);
  }

  let purged = await btp.testRunPurgeBounceTrackers();
  Assert.equal(purged.length, num, "Should have cleared num purge trackers");

  if (num == 0) {
    Assert.equal(
      Glean.bounceTrackingProtection.numHostsPerPurgeRun.testGetValue(),
      null,
      "Telemetry still be empty if no bounce trackers were detected and purged"
    );
  } else {
    Assert.equal(
      Glean.bounceTrackingProtection.numHostsPerPurgeRun.testGetValue().sum,
      num,
      "We should have recorded exactly purge with `sum` trackers"
    );

    Assert.equal(
      Glean.bounceTrackingProtection.numHostsPerPurgeRun.testGetValue().values[
        Math.min(num, 99)
      ],
      1,
      "Cleared one bounce tracker"
    );
  }

  // Cleanup
  Services.prefs.clearUserPref(
    "privacy.bounceTrackingProtection.enableDryRunMode"
  );
  Services.fog.testResetFOG();
  btp.clearAll();
}

add_task(async function test_purge_zero_dry() {
  await testNumHostsPerPurgeRun(true, 0);
});

add_task(async function test_purge_zero() {
  await testNumHostsPerPurgeRun(false, 0);
});

add_task(async function test_purge_one_dry() {
  await testNumHostsPerPurgeRun(true, 1);
});

add_task(async function test_purge_one() {
  await testNumHostsPerPurgeRun(false, 1);
});

add_task(async function test_purge_two_dry() {
  await testNumHostsPerPurgeRun(true, 2);
});

add_task(async function test_purge_two() {
  await testNumHostsPerPurgeRun(false, 2);
});

add_task(async function test_purge_99_dry() {
  await testNumHostsPerPurgeRun(true, 99);
});

add_task(async function test_purge_99() {
  await testNumHostsPerPurgeRun(false, 99);
});

add_task(async function test_purge_200_dry() {
  await testNumHostsPerPurgeRun(true, 200);
});

add_task(async function test_purge_200() {
  await testNumHostsPerPurgeRun(false, 200);
});
