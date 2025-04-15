"use strict";

const { TelemetryEnvironment } = ChromeUtils.importESModule(
  "resource://gre/modules/TelemetryEnvironment.sys.mjs"
);
const STUDIES_OPT_OUT_PREF = "app.shield.optoutstudies.enabled";
const UPLOAD_ENABLED_PREF = "datareporting.healthreport.uploadEnabled";

/**
 * FOG requires a little setup in order to test it
 */
add_setup(function test_setup() {
  // FOG needs a profile directory to put its data in.
  do_get_profile();

  // FOG needs to be initialized in order for data to flow.
  Services.fog.initializeFOG();
});

/**
 * Normal unenrollment for experiments:
 * - set .active to false
 * - set experiment inactive in telemetry
 * - send unrollment event
 */
add_task(async function test_set_inactive() {
  const manager = ExperimentFakes.manager();

  await manager.onStartup();
  await manager.store.addEnrollment(ExperimentFakes.experiment("foo"));

  manager.unenroll("foo");

  Assert.equal(
    manager.store.get("foo").active,
    false,
    "should set .active to false"
  );

  assertEmptyStore(manager.store);
});

add_task(async function test_unenroll_opt_out() {
  Services.prefs.setBoolPref(STUDIES_OPT_OUT_PREF, true);
  const manager = ExperimentFakes.manager();
  const experiment = ExperimentFakes.experiment("foo");

  // Clear any pre-existing data in Glean
  Services.fog.testResetFOG();

  await manager.onStartup();
  await manager.store.addEnrollment(experiment);

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
        branch: experiment.branch.slug,
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
        branch: experiment.branch.slug,
        reason: "studies-opt-out",
      },
    ]
  );

  assertEmptyStore(manager.store);

  // reset pref
  Services.prefs.clearUserPref(STUDIES_OPT_OUT_PREF);
});

add_task(async function test_unenroll_rollout_opt_out() {
  Services.prefs.setBoolPref(STUDIES_OPT_OUT_PREF, true);
  const manager = ExperimentFakes.manager();
  const rollout = ExperimentFakes.rollout("foo");

  // Clear any pre-existing data in Glean
  Services.fog.testResetFOG();

  await manager.onStartup();
  await manager.store.addEnrollment(rollout);

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
        branch: rollout.branch.slug,
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
        branch: rollout.branch.slug,
        reason: "studies-opt-out",
      },
    ]
  );

  assertEmptyStore(manager.store);

  // reset pref
  Services.prefs.clearUserPref(STUDIES_OPT_OUT_PREF);
});

add_task(async function test_unenroll_uploadPref() {
  const manager = ExperimentFakes.manager();
  const recipe = ExperimentFakes.recipe("foo");

  await manager.onStartup();
  await ExperimentFakes.enrollmentHelper(recipe, { manager });

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

  assertEmptyStore(manager.store);

  Services.prefs.clearUserPref(UPLOAD_ENABLED_PREF);
});

add_task(async function test_setExperimentInactive_called() {
  const sandbox = sinon.createSandbox();
  sandbox.spy(TelemetryEnvironment, "setExperimentInactive");

  const manager = ExperimentFakes.manager();
  const experiment = ExperimentFakes.recipe("foo", {
    bucketConfig: {
      ...ExperimentFakes.recipe.bucketConfig,
      count: 1000,
    },
  });

  // Clear any pre-existing data in Glean
  Services.fog.testResetFOG();

  await manager.onStartup();
  await manager.enroll(experiment);

  // Test Glean experiment API interaction
  Assert.notEqual(
    undefined,
    Services.fog.testGetExperimentData(experiment.slug),
    "experiment should be active before unenroll"
  );

  manager.unenroll("foo");

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

  assertEmptyStore(manager.store);

  sandbox.restore();
});

add_task(async function test_send_unenroll_event() {
  const manager = ExperimentFakes.manager();
  const experiment = ExperimentFakes.experiment("foo");

  // Clear any pre-existing data in Glean
  Services.fog.testResetFOG();

  await manager.onStartup();
  await manager.store.addEnrollment(experiment);

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

  manager.unenroll("foo", { reason: "some-reason" });

  // We expect only one event and that that one event matches the expected enrolled experiment
  Assert.deepEqual(
    Glean.normandy.unenrollNimbusExperiment
      .testGetValue("events")
      .map(ev => ev.extra),
    [
      {
        value: experiment.slug,
        branch: experiment.branch.slug,
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
        branch: experiment.branch.slug,
        reason: "some-reason",
      },
    ]
  );
});

add_task(async function test_undefined_reason() {
  const manager = ExperimentFakes.manager();
  const experiment = ExperimentFakes.experiment("foo");

  // Clear any pre-existing data in Glean
  Services.fog.testResetFOG();

  await manager.onStartup();
  await manager.store.addEnrollment(experiment);

  manager.unenroll("foo");

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
});

/**
 * Normal unenrollment for rollouts:
 * - remove stored enrollment and synced data (prefs)
 * - set rollout inactive in telemetry
 * - send unrollment event
 */

add_task(async function test_remove_rollouts() {
  const store = ExperimentFakes.store();
  const manager = ExperimentFakes.manager(store);
  const rollout = ExperimentFakes.rollout("foo");

  sinon.stub(store, "get").returns(rollout);
  sinon.spy(store, "updateExperiment");

  await manager.onStartup();

  manager.unenroll("foo", { reason: "some-reason" });

  Assert.ok(
    manager.store.updateExperiment.calledOnce,
    "Called to set the rollout as !active"
  );
  Assert.ok(
    manager.store.updateExperiment.calledWith(rollout.slug, {
      active: false,
      unenrollReason: "some-reason",
    }),
    "Called with expected parameters"
  );

  assertEmptyStore(manager.store);
});

add_task(async function test_unenroll_individualOptOut_statusTelemetry() {
  Services.fog.testResetFOG();

  const manager = ExperimentFakes.manager();

  await manager.onStartup();

  await manager.enroll(
    ExperimentFakes.recipe("foo", {
      bucketConfig: {
        ...ExperimentFakes.recipe.bucketConfig,
        count: 1000,
      },
      branches: [ExperimentFakes.recipe.branches[0]],
    })
  );

  Services.fog.applyServerKnobsConfig(
    JSON.stringify({
      metrics_enabled: {
        "nimbus_events.enrollment_status": true,
      },
    })
  );

  manager.unenroll("foo", { reason: "individual-opt-out" });

  Assert.deepEqual(
    Glean.nimbusEvents.enrollmentStatus
      .testGetValue("events")
      ?.map(ev => ev.extra),
    [
      {
        slug: "foo",
        branch: "control",
        status: "Disqualified",
        reason: "OptOut",
      },
    ]
  );
});
