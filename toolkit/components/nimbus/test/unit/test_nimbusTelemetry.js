/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { recordTargetingContext } = ChromeUtils.importESModule(
  "resource://nimbus/lib/TargetingContextRecorder.sys.mjs"
);

const { JsonSchema } = ChromeUtils.importESModule(
  "resource://gre/modules/JsonSchema.sys.mjs"
);

add_setup(function setup() {
  Services.fog.initializeFOG();
});

const SCHEMAS = {
  get nimbusTelemetry() {
    return fetch(
      "resource://nimbus/schemas/NimbusTelemetryFeature.schema.json",
      { credentials: "omit" }
    ).then(rsp => rsp.json());
  },
};

function nimbusTargetingContextTelemetryDisabled() {
  return !Services.prefs.getBoolPref(
    "nimbus.telemetry.targetingContextEnabled"
  );
}

function setupTest({ ...args } = {}) {
  return NimbusTestUtils.setupTest({ ...args, clearTelemetry: true });
}

add_task(
  { skip_if: nimbusTargetingContextTelemetryDisabled },
  async function test_enrollAndUnenroll_gleanMetricConfiguration() {
    info(
      "Testing the interaction of gleanMetricConfiguration with submission of enrollment status and targeting context telemetry"
    );

    const experiment = NimbusTestUtils.factories.recipe.withFeatureConfig(
      "experiment",
      {
        featureId: "nimbusTelemetry",
        value: {
          gleanMetricConfiguration: {
            metrics_enabled: {
              "nimbus_targeting_environment.targeting_context_value": true,
            },
          },
        },
      }
    );

    const { manager, cleanup } = await setupTest();

    // We don't call recordTargetingContext() here because we dont actually want
    // to do all the work when we're just testing whether or not the metrics are
    // being recorded.
    Glean.nimbusTargetingEnvironment.targetingContextValue.set(
      "nothing-active-0"
    );

    // We're submitting a bogus event here -- we shouldn't see it recorded.
    Glean.nimbusEvents.enrollmentStatus.record({ reason: "nothing-active-0" });

    Assert.equal(
      Glean.nimbusTargetingEnvironment.targetingContextValue.testGetValue(),
      null,
      "targetingContextValue not recorded by default"
    );

    Glean.nimbusTargetingEnvironment.targetingContextValue.set(
      "rollout-active-1"
    );

    Assert.equal(
      Glean.nimbusTargetingEnvironment.targetingContextValue.testGetValue(),
      null,
      "targetingContextValue not recorded by default"
    );

    await manager.enroll(experiment, "rs-loader");
    Assert.ok(
      manager.store.get(experiment.slug)?.active,
      "Experiment enrolled and active"
    );

    Glean.nimbusTargetingEnvironment.targetingContextValue.set(
      "experiment-active-2"
    );

    Assert.equal(
      Glean.nimbusTargetingEnvironment.targetingContextValue.testGetValue(),
      "experiment-active-2",
      "Targeting context metric was recorded"
    );

    // Now the listener triggers again and the metric re-enables. We should see
    // telemetry for this unenrollment but not setting the targeting context
    // value.
    await manager.unenroll(experiment.slug, { reason: "recipe-not-seen" });

    Glean.nimbusTargetingEnvironment.targetingContextValue.set(
      "rollout-active-3"
    );

    Assert.equal(
      Glean.nimbusTargetingEnvironment.targetingContextValue.testGetValue(),
      "experiment-active-2",
      "targetingContextValue was not recorded again"
    );

    Glean.nimbusTargetingEnvironment.targetingContextValue.set(
      "nothing-active-0"
    );

    Assert.equal(
      Glean.nimbusTargetingEnvironment.targetingContextValue.testGetValue(),
      "experiment-active-2",
      "targetingContextValue was not recorded again"
    );

    Services.fog.testResetFOG();

    await cleanup();
  }
);

add_task(async function test_featureConfigSchema_gleanMetricConfiguration() {
  function metricConfig(metric, value) {
    return {
      gleanMetricConfiguration: {
        metrics_enabled: {
          [metric]: value,
        },
      },
    };
  }

  const validator = new JsonSchema.Validator(await SCHEMAS.nimbusTelemetry);

  const ALLOWED = [
    "nimbus_targeting_environment.targeting_context_value",
    "nimbus_targeting_environment.pref_type_errors",
    "nimbus_targeting_environment.attr_eval_errors",
    "nimbus_targeting_environment.user_set_prefs",
    "nimbus_targeting_environment.pref_values",
    "nimbus_targeting_context.active_experiments",
    "nimbus_targeting_context.active_rollouts",
    "nimbus_targeting_context.addresses_saved",
    "nimbus_events.enrollment",
    "nimbus_events.is_ready",
    "nimbus_events.enrollment_status",
  ];

  for (const metric of ALLOWED) {
    for (const value of [true, false]) {
      Assert.ok(
        validator.validate(metricConfig(metric, value)).valid,
        `Can control ${metric} with nimbusTelemetry`
      );
    }
  }

  Assert.ok(
    !validator.validate(metricConfig("nimbus_events.enrollment", "true")).valid,
    "Schema enforces boolean values"
  );

  Assert.ok(
    !validator.validate(metricConfig("bogus", true)).valid,
    "Schema restricts metrics to nimbus metrics"
  );
});

add_task(async function test_featureConfigSchema_nimbusTargetingEnvironment() {
  const validator = new JsonSchema.Validator(await SCHEMAS.nimbusTelemetry);

  Assert.ok(
    validator.validate({
      nimbusTargetingEnvironment: {
        recordAttrs: ["activeExperiments"],
      },
    }).valid,
    "Schema validates"
  );

  Assert.ok(
    !validator.validate({
      nimbusTargetingEnvironment: {
        recordAttrs: "activeExperiments",
      },
    }).valid,
    "Schema enforces types for recordAttrs"
  );
});

add_task(
  { skip_if: nimbusTargetingContextTelemetryDisabled },
  async function test_nimbusTargetingEnvironment_recordAttrs() {
    const { manager, cleanup } = await setupTest();

    const cleanupExperiment = await NimbusTestUtils.enrollWithFeatureConfig(
      {
        featureId: "nimbusTelemetry",
        value: {
          gleanMetricConfiguration: {
            metrics_enabled: {
              "nimbus_targeting_environment.targeting_context_value": true,
            },
          },
          nimbusTargetingEnvironment: {
            recordAttrs: [
              "activeExperiments",
              "activeRollouts",
              "enrollmentsMap",
            ],
          },
        },
      },
      {
        manager,
        source: "test",
        slug: "config",
      }
    );

    Assert.equal(
      Glean.nimbusTargetingEnvironment.targetingContextValue.testGetValue(),
      null,
      "The targetingContextValue metric has not been recorded yet."
    );

    await GleanPings.nimbusTargetingContext.testSubmission(() => {
      Assert.deepEqual(
        JSON.parse(
          Glean.nimbusTargetingEnvironment.targetingContextValue.testGetValue()
        ),
        {
          activeExperiments: ["config"],
          activeRollouts: [],
          enrollmentsMap: [
            {
              experimentSlug: "config",
              branchSlug: "control",
            },
          ],
        }
      );
    }, recordTargetingContext);

    await cleanupExperiment();
    await cleanup();
  }
);
