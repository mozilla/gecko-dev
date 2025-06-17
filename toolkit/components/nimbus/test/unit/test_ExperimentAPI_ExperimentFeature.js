"use strict";

const { NimbusTelemetry } = ChromeUtils.importESModule(
  "resource://nimbus/lib/Telemetry.sys.mjs"
);
const { PrefUtils } = ChromeUtils.importESModule(
  "resource://normandy/lib/PrefUtils.sys.mjs"
);

const TEST_FALLBACK_PREF = "testprefbranch.config";

add_setup(function test_setup() {
  Services.fog.initializeFOG();
});

function setupTest(options) {
  return NimbusTestUtils.setupTest({
    ...options,
    clearTelemetry: true,
    features: [
      new ExperimentFeature("foo", {
        variables: {
          enabled: {
            type: "boolean",
            fallbackPref: "testprefbranch.enabled",
          },
          config: {
            type: "json",
            fallbackPref: TEST_FALLBACK_PREF,
          },
          remoteValue: {
            type: "boolean",
          },
          test: {
            type: "boolean",
          },
          title: {
            type: "string",
          },
        },
      }),
    ],
  });
}

add_task(async function test_ExperimentFeature_test_helper_ready() {
  const { manager, cleanup } = await setupTest();
  await manager.store.ready();

  const cleanupExperiment = await NimbusTestUtils.enrollWithFeatureConfig(
    {
      featureId: "foo",
      value: { remoteValue: "mochitest", enabled: true },
    },
    {
      manager,
      isRollout: true,
    }
  );

  Assert.equal(
    NimbusFeatures.foo.getVariable("remoteValue"),
    "mochitest",
    "set by remote config"
  );

  await cleanupExperiment();
  await cleanup();
});

add_task(async function test_record_exposure_event() {
  Services.fog.testResetFOG();

  const { sandbox, manager, cleanup } = await setupTest();

  sandbox.spy(NimbusTelemetry, "recordExposure");
  sandbox.spy(NimbusFeatures.foo, "getEnrollmentMetadata");

  await NimbusTestUtils.assert.storeIsEmpty(manager.store);
  NimbusFeatures.foo.recordExposureEvent();

  Assert.ok(
    NimbusTelemetry.recordExposure.notCalled,
    "should not emit an exposure event when no experiment is active"
  );

  // Check that there aren't any Glean exposure events yet
  var exposureEvents = Glean.nimbusEvents.exposure.testGetValue("events");
  Assert.equal(
    undefined,
    exposureEvents,
    "no Glean exposure events before exposure"
  );

  await manager.enroll(
    NimbusTestUtils.factories.recipe("blah", {
      branches: [
        {
          slug: "treatment",
          ratio: 1,
          features: [
            {
              featureId: "foo",
              value: { enabled: false },
            },
          ],
        },
      ],
    }),
    "test"
  );

  NimbusFeatures.foo.recordExposureEvent();

  Assert.ok(
    NimbusTelemetry.recordExposure.calledOnce,
    "should emit an exposure event when there is an experiment"
  );
  Assert.equal(
    NimbusFeatures.foo.getEnrollmentMetadata.callCount,
    2,
    "Should be called every time"
  );

  // Check that the Glean exposure event was recorded.
  exposureEvents = Glean.nimbusEvents.exposure.testGetValue("events");
  // We expect only one event
  Assert.equal(1, exposureEvents.length);
  // And that one event matches the expected
  Assert.equal(
    "blah",
    exposureEvents[0].extra.experiment,
    "Glean.nimbusEvents.exposure recorded with correct experiment slug"
  );
  Assert.equal(
    "treatment",
    exposureEvents[0].extra.branch,
    "Glean.nimbusEvents.exposure recorded with correct branch slug"
  );
  Assert.equal(
    "foo",
    exposureEvents[0].extra.feature_id,
    "Glean.nimbusEvents.exposure recorded with correct feature id"
  );

  sandbox.restore();

  manager.unenroll("blah");
  await cleanup();
});

add_task(async function test_record_exposure_event_once() {
  const { sandbox, manager, cleanup } = await setupTest();

  const exposureSpy = sandbox.spy(NimbusTelemetry, "recordExposure");

  await manager.enroll(
    NimbusTestUtils.factories.recipe.withFeatureConfig("blah", {
      branchSlug: "treatment",
      featureId: "foo",
      value: { enabled: false },
    }),
    "test"
  );

  NimbusFeatures.foo.recordExposureEvent({ once: true });
  NimbusFeatures.foo.recordExposureEvent({ once: true });
  NimbusFeatures.foo.recordExposureEvent({ once: true });

  Assert.ok(
    exposureSpy.calledOnce,
    "Should emit a single exposure event when the once param is true."
  );

  // Check that the Glean exposure event was recorded.
  let exposureEvents = Glean.nimbusEvents.exposure.testGetValue("events");
  // We expect only one event
  Assert.equal(1, exposureEvents.length);

  manager.unenroll("blah");
  await cleanup();
});

add_task(async function test_allow_multiple_exposure_events() {
  const { sandbox, manager, cleanup } = await setupTest();

  const exposureSpy = sandbox.spy(NimbusTelemetry, "recordExposure");

  // Clear any pre-existing data in Glean
  Services.fog.testResetFOG();

  let doExperimentCleanup = await NimbusTestUtils.enrollWithFeatureConfig(
    {
      featureId: "foo",
      value: { enabled: false },
    },
    { manager }
  );

  NimbusFeatures.foo.recordExposureEvent();
  NimbusFeatures.foo.recordExposureEvent();
  NimbusFeatures.foo.recordExposureEvent();

  Assert.equal(
    exposureSpy.callCount,
    3,
    "Should emit an exposure event for each function call"
  );

  // Check that the Glean exposure event was recorded.
  let exposureEvents = Glean.nimbusEvents.exposure.testGetValue("events");
  // We expect 3 events
  Assert.equal(3, exposureEvents.length);

  await doExperimentCleanup();
  await cleanup();
});

add_task(async function test_onUpdate_after_store_ready() {
  const { sandbox, manager, cleanup } = await setupTest();
  const stub = sandbox.stub();

  const rollout = NimbusTestUtils.factories.rollout("foo", {
    branch: {
      slug: "slug",
      features: [
        {
          featureId: "foo",
          value: {
            title: "hello",
            enabled: true,
          },
        },
      ],
    },
  });

  sandbox.stub(manager.store, "getAllActiveRollouts").returns([rollout]);

  NimbusFeatures.foo.onUpdate(stub);

  Assert.ok(stub.calledOnce, "Callback called");
  Assert.equal(stub.firstCall.args[0], "featureUpdate:foo");
  Assert.equal(stub.firstCall.args[1], "rollout-updated");

  PrefUtils.setPref(TEST_FALLBACK_PREF, JSON.stringify({ foo: true }), {
    branch: "default",
  });

  Assert.deepEqual(
    NimbusFeatures.foo.getVariable("config"),
    { foo: true },
    "Feature is ready even when initialized after store update"
  );
  Assert.equal(
    NimbusFeatures.foo.getVariable("title"),
    "hello",
    "Returns the NimbusTestUtils rollout default value"
  );

  await cleanup();
});
