/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let OA_PBM = { privateBrowsingId: 1 };
let OA_CONTAINER = { userContextId: 2 };
let OA_DEFAULT = {};

let ORIGIN_TRACKER_PBM = Services.scriptSecurityManager.createContentPrincipal(
  Services.io.newURI(ORIGIN_TRACKER),
  OA_PBM
).origin;
let ORIGIN_TRACKER_CONTAINER =
  Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI(ORIGIN_TRACKER),
    OA_CONTAINER
  ).origin;

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.bounceTrackingProtection.requireStatefulBounces", true],
      ["privacy.bounceTrackingProtection.bounceTrackingGracePeriodSec", 0],
    ],
  });
});

/**
 * Adds some test cookies in normal browsing, private browsing and in a
 * non-default user context.
 */
function addCookies() {
  info("Add a normal browsing cookie.");
  SiteDataTestUtils.addToCookies({
    host: SITE_TRACKER,
    originAttributes: OA_DEFAULT,
    name: "btp",
    value: "normal",
  });

  info("Add a private browsing cookie");
  SiteDataTestUtils.addToCookies({
    host: SITE_TRACKER,
    originAttributes: OA_PBM,
    name: "btp",
    value: "private",
  });

  info("Add a container tab cookie");
  SiteDataTestUtils.addToCookies({
    host: SITE_TRACKER,
    originAttributes: OA_CONTAINER,
    name: "btp",
    value: "container",
  });
}

// Tests that bounces in PBM don't affect state in normal browsing.
add_task(async function test_pbm_data_isolated() {
  addCookies();

  await runTestBounce({
    bounceType: "client",
    setState: "cookie-client",
    originAttributes: OA_PBM,
    skipSiteDataCleanup: true,
    postBounceCallback: () => {
      info(
        "After the PBM bounce assert that we haven't recorded any BTP data for normal browsing."
      );
      Assert.equal(
        bounceTrackingProtection.testGetBounceTrackerCandidateHosts({}).length,
        0,
        "No bounce tracker candidates for normal browsing."
      );
      Assert.equal(
        bounceTrackingProtection.testGetUserActivationHosts({}).length,
        0,
        "No user activations for normal browsing."
      );

      info("All cookies should still be present before the purge.");

      ok(
        SiteDataTestUtils.hasCookies(ORIGIN_TRACKER, [
          { key: "btp", value: "normal" },
        ]),
        "Normal browsing cookie is still set."
      );
      ok(
        SiteDataTestUtils.hasCookies(ORIGIN_TRACKER_PBM, [
          { key: "btp", value: "private" },
        ]),
        "Private browsing cookie is still set."
      );
      ok(
        SiteDataTestUtils.hasCookies(ORIGIN_TRACKER_CONTAINER, [
          { key: "btp", value: "container" },
        ]),
        "Container browsing cookie is still set."
      );
    },
  });

  ok(
    SiteDataTestUtils.hasCookies(ORIGIN_TRACKER, [
      { key: "btp", value: "normal" },
    ]),
    "Normal browsing cookie is still set."
  );
  ok(
    !SiteDataTestUtils.hasCookies(ORIGIN_TRACKER_PBM, [
      { key: "btp", value: "private" },
    ]),
    "Private browsing cookie has been cleared."
  );
  ok(
    SiteDataTestUtils.hasCookies(ORIGIN_TRACKER_CONTAINER, [
      { key: "btp", value: "container" },
    ]),
    "Container cookie is still set."
  );

  await SiteDataTestUtils.clear();
});

// Tests that bounces in PBM don't affect state in normal browsing.
add_task(async function test_containers_isolated() {
  addCookies();

  await runTestBounce({
    bounceType: "server",
    setState: "cookie-server",
    originAttributes: OA_CONTAINER,
    skipSiteDataCleanup: true,
    postBounceCallback: () => {
      // After the bounce in the container tab assert that we haven't recorded any data for normal browsing.
      Assert.equal(
        bounceTrackingProtection.testGetBounceTrackerCandidateHosts({}).length,
        0,
        "No bounce tracker candidates for normal browsing."
      );
      Assert.equal(
        bounceTrackingProtection.testGetUserActivationHosts({}).length,
        0,
        "No user activations for normal browsing."
      );

      // Or in another container tab.
      Assert.equal(
        bounceTrackingProtection.testGetBounceTrackerCandidateHosts({
          userContextId: 1,
        }).length,
        0,
        "No bounce tracker candidates for container tab 1."
      );
      Assert.equal(
        bounceTrackingProtection.testGetUserActivationHosts({
          userContextId: 1,
        }).length,
        0,
        "No user activations for container tab 1."
      );

      ok(
        SiteDataTestUtils.hasCookies(ORIGIN_TRACKER, [
          { key: "btp", value: "normal" },
        ]),
        "Normal browsing cookie is still set."
      );
      ok(
        SiteDataTestUtils.hasCookies(ORIGIN_TRACKER_PBM, [
          { key: "btp", value: "private" },
        ]),
        "Private browsing cookie is still set."
      );
      ok(
        SiteDataTestUtils.hasCookies(ORIGIN_TRACKER_CONTAINER, [
          { key: "btp", value: "container" },
        ]),
        "Container browsing cookie is still set."
      );
    },
  });

  ok(
    SiteDataTestUtils.hasCookies(ORIGIN_TRACKER, [
      { key: "btp", value: "normal" },
    ]),
    "Normal browsing cookie is still set."
  );
  ok(
    SiteDataTestUtils.hasCookies(ORIGIN_TRACKER_PBM, [
      { key: "btp", value: "private" },
    ]),
    "Private browsing cookie is still set."
  );
  ok(
    !SiteDataTestUtils.hasCookies(ORIGIN_TRACKER_CONTAINER, [
      { key: "btp", value: "container" },
    ]),
    "Container cookie has been cleared."
  );

  await SiteDataTestUtils.clear();
});
