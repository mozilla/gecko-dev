/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { ExperimentAPI } = ChromeUtils.importESModule(
  "resource://nimbus/ExperimentAPI.sys.mjs"
);

add_setup(function setup() {
  do_get_profile();
  Services.fog.initializeFOG();
});

add_task(async function test_enrollAndUnenroll() {
  const experiment = ExperimentFakes.recipe("experiment", {
    bucketConfig: {
      ...ExperimentFakes.recipe.bucketConfig,
      count: 1000,
    },
    branches: [
      {
        slug: "control",
        ratio: 1,
        features: [
          {
            featureId: "nimbusTelemetry",
            value: {
              gleanMetricConfiguration: {
                metrics_enabled: {
                  "nimbus_targeting_environment.targeting_context_value": true,
                },
              },
            },
          },
        ],
      },
    ],
  });

  const rollout = ExperimentFakes.recipe("rollout", {
    bucketConfig: experiment.bucketConfig,
    isRollout: true,
    branches: [
      {
        slug: "control",
        ratio: 1,
        features: [
          {
            featureId: "nimbusTelemetry",
            value: {
              gleanMetricConfiguration: {
                metrics_enabled: {
                  "nimbus_events.enrollment_status": true,
                },
              },
            },
          },
        ],
      },
    ],
  });

  const manager = ExperimentFakes.manager();

  sinon.stub(ExperimentAPI, "_manager").get(() => manager);
  sinon.stub(ExperimentAPI, "_store").get(() => manager.store);

  await manager.onStartup();
  await manager.store.ready();

  Glean.nimbusTargetingEnvironment.targetingContextValue.set(
    "nothing-active-0"
  );
  Glean.nimbusEvents.enrollmentStatus.record({ reason: "nothing-active-0" });

  Assert.equal(
    Glean.nimbusTargetingEnvironment.targetingContextValue.testGetValue(),
    null,
    "targetingContextValue not recorded by default"
  );
  Assert.equal(
    Glean.nimbusEvents.enrollmentStatus.testGetValue(),
    null,
    "enrollmentStatus not recorded by default"
  );

  await manager.enroll(rollout, "rs-loader");

  Glean.nimbusTargetingEnvironment.targetingContextValue.set(
    "rollout-active-1"
  );
  Glean.nimbusEvents.enrollmentStatus.record({ reason: "rollout-active-1" });

  Assert.equal(
    Glean.nimbusTargetingEnvironment.targetingContextValue.testGetValue(),
    null,
    "targetingContextValue not recorded by default"
  );

  {
    const events = Glean.nimbusEvents.enrollmentStatus.testGetValue();
    Assert.equal(events?.length ?? 0, 1, "enrollmentStatus recorded once");
    Assert.equal(
      events[0].extra.reason,
      "rollout-active-1",
      "enrollmentStatus recorded once"
    );
  }

  await manager.enroll(experiment, "rs-loader");

  Glean.nimbusTargetingEnvironment.targetingContextValue.set(
    "experiment-active-2"
  );
  Glean.nimbusEvents.enrollmentStatus.record({ reason: "experiment-active-2" });

  Assert.equal(
    Glean.nimbusTargetingEnvironment.targetingContextValue.testGetValue(),
    "experiment-active-2"
  );

  {
    const events = Glean.nimbusEvents.enrollmentStatus.testGetValue();
    Assert.equal(events?.length ?? 0, 1, "enrollmentStatus not recorded again");
  }

  await manager.unenroll("experiment");

  Glean.nimbusTargetingEnvironment.targetingContextValue.set(
    "rollout-active-3"
  );
  Glean.nimbusEvents.enrollmentStatus.record({ reason: "rollout-active-3" });

  Assert.equal(
    Glean.nimbusTargetingEnvironment.targetingContextValue.testGetValue(),
    "experiment-active-2",
    "targetingContextValue was not recorded again"
  );

  {
    const events = Glean.nimbusEvents.enrollmentStatus.testGetValue();
    Assert.equal(events?.length ?? 0, 2, "enrollmentStatus recorded again");
    Assert.equal(
      events[1].extra.reason,
      "rollout-active-3",
      "enrollmentStatus recorded with correct value"
    );
  }

  await manager.unenroll("rollout");

  Glean.nimbusTargetingEnvironment.targetingContextValue.set(
    "nothing-active-0"
  );
  Glean.nimbusEvents.enrollmentStatus.record({ reason: "nothing-active-0" });

  Assert.equal(
    Glean.nimbusTargetingEnvironment.targetingContextValue.testGetValue(),
    "experiment-active-2",
    "targetingContextValue was not recorded again"
  );

  {
    const events = Glean.nimbusEvents.enrollmentStatus.testGetValue();
    Assert.equal(events?.length ?? 0, 2, "enrollmentStatus not recorded again");
  }

  Services.fog.testResetFOG();

  manager.store._deleteForTests("experiment");
  manager.store._deleteForTests("rollout");

  await assertEmptyStore(manager.store);
});
