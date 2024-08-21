/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// The test needs to open new PBM windows  which is slow on debug builds.
requestLongerTimeout(3);

/**
 * Helper function for testing that BTP gets enabled/disabled for a specific
 * cookie behavior.
 *
 * @param {Number} cookieBehavior - One of Ci.nsICookieService.BEHAVIOR* values.
 * @param {Number} privateBrowsingId - Run test in private/non-private mode.
 */
async function runTestCookieBehavior(
  cookieBehavior,
  privateBrowsingId,
  shouldBeEnabled
) {
  info(
    "runTestCookieBehavior " +
      JSON.stringify({ cookieBehavior, privateBrowsingId, shouldBeEnabled })
  );
  if (privateBrowsingId == 0) {
    await SpecialPowers.pushPrefEnv({
      set: [["network.cookie.cookieBehavior", cookieBehavior]],
    });
  } else {
    await SpecialPowers.pushPrefEnv({
      set: [["network.cookie.cookieBehavior.pbmode", cookieBehavior]],
    });
  }

  await runTestBounce({
    bounceType: "server",
    setState: "cookie-server",
    originAttributes: {
      privateBrowsingId,
    },
    expectRecordBounces: shouldBeEnabled,
    expectCandidate: shouldBeEnabled,
    expectPurge: shouldBeEnabled,
  });

  // Cleanup
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
 * Tests classification + purging with different cookie behavior settings.
 */
add_task(async function test_cookie_behaviors() {
  for (let pbId = 0; pbId < 2; pbId++) {
    // BTP is disabled
    await runTestCookieBehavior(
      Ci.nsICookieService.BEHAVIOR_ACCEPT,
      pbId,
      false
    );
    await runTestCookieBehavior(
      Ci.nsICookieService.BEHAVIOR_REJECT,
      pbId,
      false
    );

    // BTP is enabled
    await runTestCookieBehavior(
      Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN,
      pbId,
      true
    );
    await runTestCookieBehavior(
      Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN,
      pbId,
      true
    );
    await runTestCookieBehavior(
      Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
      pbId,
      true
    );
    await runTestCookieBehavior(
      Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
      pbId,
      true
    );
  }
  Assert.equal(
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
    Ci.nsICookieService.BEHAVIOR_LAST,
    "test covers all cookie behaviors"
  );
});
