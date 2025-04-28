/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { NetUtil } = ChromeUtils.importESModule(
  "resource://gre/modules/NetUtil.sys.mjs"
);
const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);

/**
 * Tests that a visit to about:welcome results in an exposure recorded via
 * Nimbus / Glean.
 */

add_task(async function test_exposure() {
  do_get_profile();
  Services.fog.initializeFOG();
  ExperimentFakes.cleanupStorePrefCache();

  // Simulate a visit by requesting the about:welcome nsIAboutModule, and
  // requesting a channel for about:welcome.
  let module = Cc[
    "@mozilla.org/network/protocol/about;1?what=welcome"
  ].getService(Ci.nsIAboutModule);
  Assert.ok(module, "Found the nsIAboutModule.");

  Services.fog.testResetFOG();
  Assert.ok(
    !Glean.normandy.exposeNimbusExperiment.testGetValue(),
    "No exposure events recorded yet."
  );

  let store = ExperimentFakes.store();
  let experiment = ExperimentFakes.experiment("foo", {
    features: [{ featureId: "aboutwelcome", isEarlyStartup: true }],
  });

  await store.init();
  store.addEnrollment(experiment);

  const ABOUT_WELCOME_URI = Services.io.newURI("about:welcome");

  // create a dummy loadinfo which we can hand to newChannel.
  let dummyChannel = NetUtil.newChannel({
    uri: Services.io.newURI("http://localhost"),
    loadUsingSystemPrincipal: true,
  });
  let dummyLoadInfo = dummyChannel.loadInfo;
  module.newChannel(ABOUT_WELCOME_URI, dummyLoadInfo);

  let exposureEvents = Glean.normandy.exposeNimbusExperiment.testGetValue();
  Assert.ok(exposureEvents, "Found exposure events.");
  let result = exposureEvents.filter(exposureEvent => {
    return (
      exposureEvent.category === "normandy" &&
      exposureEvent.extra.featureId === "aboutwelcome"
    );
  });
  Assert.equal(result.length, 1, "Found the single exposure");

  // Ensure there we only record this once.
  module.newChannel(ABOUT_WELCOME_URI, dummyLoadInfo);

  result = exposureEvents.filter(exposureEvent => {
    return (
      exposureEvent.category === "normandy" &&
      exposureEvent.extra.featureId === "aboutwelcome"
    );
  });
  Assert.equal(result.length, 1, "Only a single exposure still.");
});
