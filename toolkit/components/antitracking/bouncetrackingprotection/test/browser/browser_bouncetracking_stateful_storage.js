/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let bounceTrackingProtection;

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.bounceTrackingProtection.requireStatefulBounces", true],
      ["privacy.bounceTrackingProtection.bounceTrackingGracePeriodSec", 0],
    ],
  });
  bounceTrackingProtection = Cc[
    "@mozilla.org/bounce-tracking-protection;1"
  ].getService(Ci.nsIBounceTrackingProtection);
});

// Storage tests.

add_task(async function test_bounce_stateful_localStorage() {
  info("Client bounce with localStorage.");
  await runTestBounce({
    bounceType: "client",
    setState: "localStorage",
  });
});

add_task(async function test_bounce_stateful_localStorage_sameSiteFrame() {
  info("Client bounce with localStorage set in same site frame.");
  await runTestBounce({
    bounceType: "client",
    setState: "localStorage",
    setStateSameSiteFrame: true,
  });
});

add_task(async function test_bounce_stateful_indexedDB() {
  info("Client bounce with indexedDB.");
  await runTestBounce({
    bounceType: "client",
    setState: "indexedDB",
  });
});

add_task(async function test_bounce_stateful_indexedDB_sameSiteFrame() {
  info("Client bounce with indexedDB populated in same site frame.");
  await runTestBounce({
    bounceType: "client",
    setState: "indexedDB",
    setStateSameSiteFrame: true,
  });
});

add_task(async function test_bounce_stateful_localStorage_crossSiteFrame() {
  info("Test client bounce with localStorage set in a third-party iframe.");
  await runTestBounce({
    bounceType: "client",
    setState: "localStorage",
    setStateCrossSiteFrame: true,
  });
});

add_task(async function test_bounce_stateful_indexedDB_crossSiteFrame() {
  info("Test client bounce with indexedDB set in a third-party iframe.");
  await runTestBounce({
    bounceType: "client",
    setState: "indexedDB",
    setStateCrossSiteFrame: true,
  });
});
