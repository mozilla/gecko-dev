"use strict";

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

const TELEMETRY_CATEGORY = "normandy";
const TELEMETRY_OBJECT = "nimbus_experiment";
// Included with active experiment information
const EXPERIMENT_TYPE = "nimbus";
const EVENT_FILTER = { category: TELEMETRY_CATEGORY };

const TEST_FEATURE = new ExperimentFeature("test-feature", {
  description: "Test feature",
  exposureDescription: "Used in tests",
});

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["app.shield.optoutstudies.enabled", true]],
  });

  const cleanupFeature = NimbusTestUtils.addTestFeatures(TEST_FEATURE);

  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
    cleanupFeature();
  });
});

add_task(async function test_experiment_enroll_unenroll_Telemetry() {
  Services.telemetry.clearEvents();
  const cleanup = await NimbusTestUtils.enrollWithFeatureConfig({
    featureId: TEST_FEATURE.featureId,
    value: { enabled: false },
  });

  const metadata =
    NimbusFeatures[TEST_FEATURE.featureId].getEnrollmentMetadata();

  Assert.ok(!!metadata, "Should be enrolled in the experiment");
  TelemetryTestUtils.assertEvents(
    [
      {
        method: "enroll",
        object: TELEMETRY_OBJECT,
        value: metadata.slug,
        extra: {
          experimentType: EXPERIMENT_TYPE,
          branch: metadata.branch,
        },
      },
    ],
    EVENT_FILTER
  );

  cleanup();

  TelemetryTestUtils.assertEvents(
    [
      {
        method: "unenroll",
        object: TELEMETRY_OBJECT,
        value: metadata.slug,
        extra: {
          reason: "unknown",
          branch: metadata.branch,
        },
      },
    ],
    EVENT_FILTER
  );
});

add_task(async function test_experiment_expose_Telemetry() {
  const cleanup = await NimbusTestUtils.enrollWithFeatureConfig({
    featureId: TEST_FEATURE.featureId,
    value: { enabled: false },
  });

  const meta = NimbusFeatures[TEST_FEATURE.featureId].getEnrollmentMetadata();

  Services.telemetry.clearEvents();
  TEST_FEATURE.recordExposureEvent();

  TelemetryTestUtils.assertEvents(
    [
      {
        method: "expose",
        object: TELEMETRY_OBJECT,
        value: meta.slug,
        extra: {
          branchSlug: meta.branch,
          featureId: TEST_FEATURE.featureId,
        },
      },
    ],
    EVENT_FILTER
  );

  cleanup();
});

add_task(async function test_rollout_expose_Telemetry() {
  const cleanup = await NimbusTestUtils.enrollWithFeatureConfig(
    {
      featureId: TEST_FEATURE.featureId,
      value: { enabled: false },
    },
    { isRollout: true }
  );

  const meta = NimbusFeatures[TEST_FEATURE.featureId].getEnrollmentMetadata();

  Assert.ok(!!meta, "Found enrolled experiment");

  Services.telemetry.clearEvents();
  TEST_FEATURE.recordExposureEvent();

  TelemetryTestUtils.assertEvents(
    [
      {
        method: "expose",
        object: TELEMETRY_OBJECT,
        value: meta.slug,
        extra: {
          branchSlug: meta.branch,
          featureId: TEST_FEATURE.featureId,
        },
      },
    ],
    EVENT_FILTER
  );

  cleanup();
});
