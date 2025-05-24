"use strict";

const { TelemetryEnvironment } = ChromeUtils.importESModule(
  "resource://gre/modules/TelemetryEnvironment.sys.mjs"
);
const STUDIES_OPT_OUT_PREF = "app.shield.optoutstudies.enabled";
const UPLOAD_ENABLED_PREF = "datareporting.healthreport.uploadEnabled";

add_setup(function test_setup() {
  Services.fog.initializeFOG();
});

function setupTest({ ...args } = {}) {
  return NimbusTestUtils.setupTest({ ...args, clearTelemetry: true });
}

/**
 * Normal unenrollment for experiments:
 * - set .active to false
 * - set experiment inactive in telemetry
 * - send unrollment event
 */
add_task(async function test_set_inactive() {
  const { manager, cleanup } = await setupTest();

  await manager.enroll(NimbusTestUtils.factories.recipe("foo"), "test");
  await manager.unenroll("foo");

  Assert.equal(
    manager.store.get("foo").active,
    false,
    "should set .active to false"
  );

  await cleanup();
});

add_task(async function test_unenroll_opt_out() {
  Services.prefs.setBoolPref(STUDIES_OPT_OUT_PREF, true);

  const { manager, cleanup } = await setupTest();
  const experiment = NimbusTestUtils.factories.recipe.withFeatureConfig("foo", {
    featureId: "testFeature",
  });
  await manager.enroll(experiment, "test");

  // Check that there aren't any Glean normandy unenrollNimbusExperiment events yet
  Assert.equal(
    Glean.normandy.unenrollNimbusExperiment.testGetValue("events"),
    undefined,
    "no Glean normandy unenrollNimbusExperiment events before unenrollment"
  );

  // Check that there aren't any Glean unenrollment events yet
  Assert.equal(
    Glean.nimbusEvents.unenrollment.testGetValue("events"),
    undefined,
    "no Glean unenrollment events before unenrollment"
  );

  Services.prefs.setBoolPref(STUDIES_OPT_OUT_PREF, false);

  Assert.equal(
    manager.store.get(experiment.slug).active,
    false,
    "should set .active to false"
  );

  // We expect only one event and that that one event matches the expected enrolled experiment
  Assert.deepEqual(
    Glean.normandy.unenrollNimbusExperiment
      .testGetValue("events")
      .map(ev => ev.extra),
    [
      {
        value: experiment.slug,
        branch: experiment.branches[0].slug,
        reason: "studies-opt-out",
      },
    ]
  );

  // We expect only one event and that that one event matches the expected enrolled experiment
  Assert.deepEqual(
    Glean.nimbusEvents.unenrollment.testGetValue("events").map(ev => ev.extra),
    [
      {
        experiment: experiment.slug,
        branch: experiment.branches[0].slug,
        reason: "studies-opt-out",
      },
    ]
  );

  await cleanup();
  Services.prefs.clearUserPref(STUDIES_OPT_OUT_PREF);
});

add_task(async function test_unenroll_rollout_opt_out() {
  Services.prefs.setBoolPref(STUDIES_OPT_OUT_PREF, true);

  const { manager, cleanup } = await setupTest();
  const rollout = NimbusTestUtils.factories.recipe("foo", { isRollout: true });
  await manager.enroll(rollout, "test");

  // Check that there aren't any Glean normandy unenrollNimbusExperiment events yet
  Assert.equal(
    Glean.normandy.unenrollNimbusExperiment.testGetValue("events"),
    undefined,
    "no Glean normandy unenrollNimbusExperiment events before unenrollment"
  );

  // Check that there aren't any Glean unenrollment events yet
  Assert.equal(
    Glean.nimbusEvents.unenrollment.testGetValue("events"),
    undefined,
    "no Glean unenrollment events before unenrollment"
  );

  Services.prefs.setBoolPref(STUDIES_OPT_OUT_PREF, false);

  Assert.equal(
    manager.store.get(rollout.slug).active,
    false,
    "should set .active to false"
  );

  // We expect only one event and that that one event matches the expected enrolled experiment
  Assert.deepEqual(
    Glean.normandy.unenrollNimbusExperiment
      .testGetValue("events")
      .map(ev => ev.extra),
    [
      {
        value: rollout.slug,
        branch: rollout.branches[0].slug,
        reason: "studies-opt-out",
      },
    ]
  );

  // We expect only one event and that that one event matches the expected enrolled experiment
  Assert.deepEqual(
    Glean.nimbusEvents.unenrollment.testGetValue("events").map(ev => ev.extra),
    [
      {
        experiment: rollout.slug,
        branch: rollout.branches[0].slug,
        reason: "studies-opt-out",
      },
    ]
  );

  await cleanup();
  Services.prefs.clearUserPref(STUDIES_OPT_OUT_PREF);
});

add_task(async function test_unenroll_uploadPref() {
  const { manager, cleanup } = await setupTest();
  const recipe = NimbusTestUtils.factories.recipe("foo");

  await manager.store.init();
  await manager.onStartup();
  await NimbusTestUtils.enroll(recipe, { manager });

  Assert.equal(
    manager.store.get(recipe.slug).active,
    true,
    "Should set .active to true"
  );

  Services.prefs.setBoolPref(UPLOAD_ENABLED_PREF, false);

  Assert.equal(
    manager.store.get(recipe.slug).active,
    false,
    "Should set .active to false"
  );

  await cleanup();
  Services.prefs.clearUserPref(UPLOAD_ENABLED_PREF);
});

add_task(async function test_setExperimentInactive_called() {
  const { sandbox, manager, cleanup } = await setupTest();
  sandbox.spy(TelemetryEnvironment, "setExperimentInactive");

  const experiment = NimbusTestUtils.factories.recipe("foo");

  await manager.enroll(experiment, "test");

  // Test Glean experiment API interaction
  Assert.notEqual(
    undefined,
    Services.fog.testGetExperimentData(experiment.slug),
    "experiment should be active before unenroll"
  );

  await manager.unenroll("foo");

  Assert.ok(
    TelemetryEnvironment.setExperimentInactive.calledWith("foo"),
    "should call TelemetryEnvironment.setExperimentInactive with slug"
  );

  // Test Glean experiment API interaction
  Assert.equal(
    undefined,
    Services.fog.testGetExperimentData(experiment.slug),
    "experiment should be inactive after unenroll"
  );

  await cleanup();
});

add_task(async function test_send_unenroll_event() {
  const { manager, cleanup } = await setupTest();
  const experiment = NimbusTestUtils.factories.recipe.withFeatureConfig("foo", {
    featureId: "testFeature",
  });

  await manager.enroll(experiment, "test");

  // Check that there aren't any Glean normandy unenrollNimbusExperiment events yet
  Assert.equal(
    Glean.normandy.unenrollNimbusExperiment.testGetValue("events"),
    undefined,
    "no Glean normandy unenrollNimbusExperiment events before unenrollment"
  );

  // Check that there aren't any Glean unenrollment events yet
  Assert.equal(
    Glean.nimbusEvents.unenrollment.testGetValue("events"),
    undefined,
    "no Glean unenrollment events before unenrollment"
  );

  await manager.unenroll("foo", { reason: "some-reason" });

  // We expect only one event and that that one event matches the expected enrolled experiment
  Assert.deepEqual(
    Glean.normandy.unenrollNimbusExperiment
      .testGetValue("events")
      .map(ev => ev.extra),
    [
      {
        value: experiment.slug,
        branch: experiment.branches[0].slug,
        reason: "some-reason",
      },
    ]
  );

  // We expect only one event and that that one event matches the expected enrolled experiment
  Assert.deepEqual(
    Glean.nimbusEvents.unenrollment.testGetValue("events").map(ev => ev.extra),
    [
      {
        experiment: experiment.slug,
        branch: experiment.branches[0].slug,
        reason: "some-reason",
      },
    ]
  );

  await cleanup();
});

add_task(async function test_undefined_reason() {
  const { manager, cleanup } = await setupTest();
  const experiment = NimbusTestUtils.factories.recipe("foo");

  await manager.enroll(experiment, "test");

  await manager.unenroll("foo");

  // We expect only one event and that that one event reason matches the expected reason
  Assert.deepEqual(
    Glean.normandy.unenrollNimbusExperiment
      .testGetValue("events")
      .map(ev => ev.extra.reason),
    ["unknown"]
  );

  // We expect only one event and that that one event reason matches the expected reason
  Assert.deepEqual(
    Glean.nimbusEvents.unenrollment
      .testGetValue("events")
      .map(ev => ev.extra.reason),
    ["unknown"]
  );

  await cleanup();
});

/**
 * Normal unenrollment for rollouts:
 * - remove stored enrollment and synced data (prefs)
 * - set rollout inactive in telemetry
 * - send unrollment event
 */

add_task(async function test_remove_rollouts() {
  const { sandbox, manager, cleanup } = await setupTest();
  sandbox.spy(manager.store, "updateExperiment");
  const rollout = NimbusTestUtils.factories.rollout("foo");

  await manager.enroll(
    NimbusTestUtils.factories.recipe("foo", { isRollout: true }),
    "test"
  );
  Assert.ok(
    manager.store.updateExperiment.notCalled,
    "Should not have called updateExperiment when enrolling"
  );

  await manager.unenroll("foo", { reason: "some-reason" });

  Assert.ok(
    manager.store.updateExperiment.calledOnce,
    "Called to set the rollout as inactive"
  );
  Assert.ok(
    manager.store.updateExperiment.calledWith(rollout.slug, {
      active: false,
      unenrollReason: "some-reason",
    }),
    "Called with expected parameters"
  );

  await cleanup();
});

add_task(async function test_unenroll_individualOptOut_statusTelemetry() {
  const { manager, cleanup } = await setupTest();

  await manager.enroll(
    NimbusTestUtils.factories.recipe.withFeatureConfig("foo", {
      featureId: "testFeature",
    }),
    "test"
  );

  await manager.unenroll("foo", { reason: "individual-opt-out" });

  Assert.deepEqual(
    Glean.nimbusEvents.enrollmentStatus
      .testGetValue("events")
      ?.map(ev => ev.extra),
    [
      {
        status: "Enrolled",
        reason: "Qualified",
        slug: "foo",
        branch: "control",
      },
      {
        branch: "control",
        reason: "OptOut",
        status: "Disqualified",
        slug: "foo",
      },
    ]
  );

  await cleanup();
});

add_task(async function testUnenrollBogusReason() {
  const { manager, cleanup } = await setupTest();

  await manager.enroll(
    NimbusTestUtils.factories.recipe("bogus", {
      branches: [NimbusTestUtils.factories.recipe.branches[0]],
    }),
    "test"
  );

  Assert.ok(manager.store.get("bogus").active, "Enrollment active");

  await manager.unenroll("bogus", "bogus");

  Assert.deepEqual(
    Glean.nimbusEvents.enrollmentStatus
      .testGetValue("events")
      ?.map(ev => ev.extra),
    [
      {
        branch: "control",
        status: "Enrolled",
        reason: "Qualified",
        slug: "bogus",
      },
      {
        status: "Disqualified",
        slug: "bogus",
        reason: "Error",
        error_string: "unknown",
        branch: "control",
      },
    ]
  );

  Assert.deepEqual(
    Glean.nimbusEvents.unenrollment.testGetValue("events")?.map(ev => ev.extra),
    [
      {
        experiment: "bogus",
        branch: "control",
        reason: "unknown",
      },
    ]
  );

  Assert.deepEqual(
    Glean.normandy.unenrollNimbusExperiment
      .testGetValue("events")
      ?.map(ev => ev.extra),
    [
      {
        value: "bogus",
        branch: "control",
        reason: "unknown",
      },
    ]
  );

  await cleanup();
});
