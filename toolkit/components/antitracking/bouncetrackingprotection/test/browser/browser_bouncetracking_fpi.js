/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let bounceTrackingProtection = Cc[
  "@mozilla.org/bounce-tracking-protection;1"
].getService(Ci.nsIBounceTrackingProtection);

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "privacy.bounceTrackingProtection.mode",
        Ci.nsIBounceTrackingProtection.MODE_ENABLED,
      ],
      ["privacy.bounceTrackingProtection.bounceTrackingGracePeriodSec", 0],
      ["privacy.firstparty.isolate", true],
    ],
  });
  bounceTrackingProtection.clearAll();
});

// Tests that BTP works as expected with first party isolation enabled.
add_task(async function test_fpi_bounce_simple() {
  await runTestBounce({
    bounceType: "server",
    setState: "cookie-server",
  });
});

// Tests that user activation protects a site from being purged when FPI is
// enabled.
add_task(async function test_fpi_user_activation_protects_site() {
  is(
    bounceTrackingProtection.testGetUserActivationHosts({}).length,
    0,
    "Should have no user activation hosts initially."
  );

  await BrowserTestUtils.withNewTab(ORIGIN_TRACKER, async browser => {
    info("Interact with the tracker site.");
    await BrowserTestUtils.synthesizeMouseAtPoint(1, 1, {}, browser);
  });

  // This assertion has limited utility, since the internal
  // BounceTrackingStateGlobal getter which is used by
  // testGetUserActivationHosts strips out first party domain.
  info(
    "Test that user activation has been recorded for the default OriginAttributes dictionary."
  );
  let userActivationHosts = bounceTrackingProtection.testGetUserActivationHosts(
    {}
  );
  is(userActivationHosts.length, 1, "Should have one user activation.");
  is(
    userActivationHosts[0].siteHost,
    SITE_TRACKER,
    `User activation host is ${SITE_TRACKER}.`
  );

  info(
    "Run a bounce and assert that the site is not purged because it's protected by the recent user activation."
  );
  await runTestBounce({
    bounceType: "server",
    setState: "cookie-server",
    expectCandidate: false,
    expectPurge: false,
  });

  bounceTrackingProtection.clearAll();
});
