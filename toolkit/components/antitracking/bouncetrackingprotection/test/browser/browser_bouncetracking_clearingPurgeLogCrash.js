/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Verifies that clearing the purge log via nsIClearDataService does not lead to
 * a crash.
 * See https://bugzilla.mozilla.org/show_bug.cgi?id=1947281#c2 for details.
 */

async function bounceTwice() {
  for (let i = 0; i < 2; i++) {
    await runTestBounce({
      bounceType: "client",
      setState: "cookie-client",
      // We don't want to clear the purge log here because we want to test that
      // clearing the purge log through nsIClearDataService does not lead to a
      // crash.
      skipBounceTrackingProtectionCleanup: true,
      // This also calls into BTP clearing.
      skipSiteDataCleanup: true,
      // Skip state checks on the second bounce. The second test run would
      // otherwise fail because it expects to start with clean BTP state.
      skipStateChecks: i == 1,
    });
  }
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.bounceTrackingProtection.requireStatefulBounces", true],
      ["privacy.bounceTrackingProtection.bounceTrackingGracePeriodSec", 0],
    ],
  });
});

add_task(async function test_crash() {
  info("Bounce and purge twice so we get two entries in recent purges.");
  await bounceTwice();

  info("Clear BTP state via nsIClearDataService which must not crash.");
  await new Promise(function (resolve) {
    Services.clearData.deleteDataInTimeRange(
      0,
      // In microseconds
      Date.now() * 1000,
      true,
      Ci.nsIClearDataService.CLEAR_BOUNCE_TRACKING_PROTECTION_STATE,
      failedFlags => {
        Assert.equal(failedFlags, 0, "Clearing should have succeeded");
        resolve();
      }
    );
  });

  // Cleanup
  await SiteDataTestUtils.clear();
});
