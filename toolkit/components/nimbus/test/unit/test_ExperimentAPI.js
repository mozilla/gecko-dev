"use strict";

const { NimbusTelemetry } = ChromeUtils.importESModule(
  "resource://nimbus/lib/Telemetry.sys.mjs"
);
const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);
const COLLECTION_ID_PREF = "messaging-system.rsexperimentloader.collection_id";

add_setup(function () {
  Services.fog.initializeFOG();
});

/**
 * #getRecipe
 */
add_task(async function test_getRecipe() {
  const { sandbox, cleanup } = await NimbusTestUtils.setupTest();
  const RECIPE = NimbusTestUtils.factories.recipe("foo");
  const collectionName = Services.prefs.getStringPref(COLLECTION_ID_PREF);
  sandbox.stub(ExperimentAPI._remoteSettingsClient, "get").resolves([RECIPE]);

  const recipe = await ExperimentAPI.getRecipe("foo");
  Assert.deepEqual(
    recipe,
    RECIPE,
    "should return an experiment recipe if found"
  );
  Assert.equal(
    ExperimentAPI._remoteSettingsClient.collectionName,
    collectionName,
    "Loaded the expected collection"
  );

  await cleanup();
});

add_task(async function test_getRecipe_Failure() {
  const { sandbox, cleanup } = await NimbusTestUtils.setupTest();
  sandbox.stub(ExperimentAPI._remoteSettingsClient, "get").throws();

  const recipe = await ExperimentAPI.getRecipe("foo");
  Assert.equal(recipe, undefined, "should return undefined if RS throws");

  await cleanup();
});

/**
 * #getAllBranches
 */
add_task(async function test_getAllBranches() {
  const { sandbox, cleanup } = await NimbusTestUtils.setupTest();
  const RECIPE = NimbusTestUtils.factories.recipe("foo");
  sandbox.stub(ExperimentAPI._remoteSettingsClient, "get").resolves([RECIPE]);

  const branches = await ExperimentAPI.getAllBranches("foo");
  Assert.deepEqual(
    branches,
    RECIPE.branches,
    "should return all branches if found a recipe"
  );

  await cleanup();
});

// API used by Messaging System
add_task(async function test_getAllBranches_featureIdAccessor() {
  const { sandbox, cleanup } = await NimbusTestUtils.setupTest();

  const RECIPE = NimbusTestUtils.factories.recipe("foo");
  sandbox.stub(ExperimentAPI._remoteSettingsClient, "get").resolves([RECIPE]);

  const branches = await ExperimentAPI.getAllBranches("foo");
  Assert.deepEqual(
    branches,
    RECIPE.branches,
    "should return all branches if found a recipe"
  );
  branches.forEach(branch => {
    Assert.equal(
      branch.testFeature.featureId,
      "testFeature",
      "Should use the experimentBranchAccessor proxy getter"
    );
  });

  await cleanup();
});

// For schema version before 1.6.2 branch.feature was accessed
// instead of branch.features
add_task(async function test_getAllBranches_backwardsCompat() {
  const { sandbox, cleanup } = await NimbusTestUtils.setupTest();

  const RECIPE = NimbusTestUtils.factories.recipe("foo");
  delete RECIPE.branches[0].features;
  delete RECIPE.branches[1].features;
  let feature = {
    featureId: "backwardsCompat",
    value: {
      enabled: true,
    },
  };
  RECIPE.branches[0].feature = feature;
  RECIPE.branches[1].feature = feature;
  sandbox.stub(ExperimentAPI._remoteSettingsClient, "get").resolves([RECIPE]);

  const branches = await ExperimentAPI.getAllBranches("foo");
  Assert.deepEqual(
    branches,
    RECIPE.branches,
    "should return all branches if found a recipe"
  );
  branches.forEach(branch => {
    Assert.equal(
      branch.backwardsCompat.featureId,
      "backwardsCompat",
      "Should use the experimentBranchAccessor proxy getter"
    );
  });

  await cleanup();
});

add_task(async function test_getAllBranches_Failure() {
  const { sandbox, cleanup } = await NimbusTestUtils.setupTest();

  sandbox.stub(ExperimentAPI._remoteSettingsClient, "get").throws();

  const branches = await ExperimentAPI.getAllBranches("foo");
  Assert.equal(branches, undefined, "should return undefined if RS throws");

  await cleanup();
});

/**
 * Store events
 */
add_task(async function test_addEnrollment_eventEmit_add() {
  const { sandbox, manager, cleanup } = await NimbusTestUtils.setupTest();
  const store = manager.store;

  const featureStub = sandbox.stub();
  const experiment = NimbusTestUtils.factories.experiment("foo", {
    branch: {
      slug: "variant",
      ratio: 1,
      features: [{ featureId: "purple", value: {} }],
    },
  });

  await ExperimentAPI.ready();

  store.on("featureUpdate:purple", featureStub);

  store.addEnrollment(experiment);

  Assert.equal(
    featureStub.callCount,
    1,
    "should call 'featureUpdate' callback for featureId when an experiment is added"
  );
  Assert.equal(featureStub.firstCall.args[0], "featureUpdate:purple");
  Assert.equal(featureStub.firstCall.args[1], "experiment-updated");

  store.off("featureUpdate:purple", featureStub);

  await manager.unenroll(experiment.slug);
  await cleanup();
});

add_task(async function test_updateExperiment_eventEmit_add_and_update() {
  const { sandbox, manager, cleanup } = await NimbusTestUtils.setupTest();
  const store = manager.store;

  const featureStub = sandbox.stub();
  const experiment = NimbusTestUtils.factories.experiment("foo", {
    branch: {
      slug: "variant",
      ratio: 1,
      features: [{ featureId: "purple", value: {} }],
    },
  });

  store.addEnrollment(experiment);

  store._onFeatureUpdate("purple", featureStub);

  store.updateExperiment(experiment.slug, experiment);

  await TestUtils.waitForCondition(
    () => featureStub.callCount == 2,
    "Wait for `on` method to notify callback about the `add` event."
  );
  // Called twice, once when attaching the event listener (because there is an
  // existing experiment with that name) and 2nd time for the update event
  Assert.equal(featureStub.callCount, 2, "Called twice for feature");
  Assert.equal(featureStub.firstCall.args[0], "featureUpdate:purple");
  Assert.equal(featureStub.firstCall.args[1], "experiment-updated");

  store._offFeatureUpdate("featureUpdate:purple", featureStub);

  await manager.unenroll(experiment.slug);
  await cleanup();
});

add_task(async function test_updateExperiment_eventEmit_off() {
  const { manager, sandbox, cleanup } = await NimbusTestUtils.setupTest();
  const store = manager.store;

  const featureStub = sandbox.stub();
  const experiment = NimbusTestUtils.factories.experiment("foo", {
    branch: {
      slug: "variant",
      ratio: 1,
      features: [{ featureId: "purple", value: {} }],
    },
  });

  store.on("featureUpdate:purple", featureStub);

  store.addEnrollment(experiment);

  store.off("featureUpdate:purple", featureStub);

  store.updateExperiment(experiment.slug, experiment);

  Assert.equal(featureStub.callCount, 1, "Called only once before `off`");

  await manager.unenroll(experiment.slug);
  await cleanup();
});

add_task(async function testGetEnrollments() {
  const { manager, cleanup } = await NimbusTestUtils.setupTest();

  const fallbackPref = "nimbus.test-only.feature.baz";
  const cleanupFeature = NimbusTestUtils.addTestFeatures(
    new ExperimentFeature("foo", {
      variables: {
        foo: { type: "int" },
        bar: { type: "boolean" },
        baz: {
          type: "string",
          fallbackPref,
        },
      },
    })
  );

  Services.prefs.setStringPref(fallbackPref, "pref-value");

  await manager.enroll(
    NimbusTestUtils.factories.recipe.withFeatureConfig("experiment", {
      branchSlug: "treatment",
      featureId: "foo",
      value: { foo: 1, baz: "qux" },
    }),
    "test"
  );
  await manager.enroll(
    NimbusTestUtils.factories.recipe.withFeatureConfig(
      "rollout",
      { featureId: "foo", value: { foo: 2, bar: false } },
      { isRollout: true }
    ),
    "test"
  );

  const enrollments = NimbusFeatures.foo
    .getAllEnrollments()
    .sort((a, b) => a.meta.slug.localeCompare(b.meta.slug));

  Assert.deepEqual(
    enrollments,
    [
      {
        meta: {
          slug: "experiment",
          branch: "treatment",
          isRollout: false,
        },
        value: {
          foo: 1,
          baz: "qux",
        },
      },
      {
        meta: {
          slug: "rollout",
          branch: "control",
          isRollout: true,
        },
        value: {
          foo: 2,
          bar: false,
          baz: "pref-value",
        },
      },
    ],
    "Should have two enrollments"
  );

  await NimbusTestUtils.cleanupManager(["experiment", "rollout"], { manager });
  Services.prefs.clearUserPref(fallbackPref);
  cleanupFeature();
  await cleanup();
});

add_task(async function testGetEnrollmentsCoenrolling() {
  const { manager, cleanup } = await NimbusTestUtils.setupTest();

  const fallbackPref = "nimbus.test-only.feature.baz";
  const cleanupFeature = NimbusTestUtils.addTestFeatures(
    new ExperimentFeature("foo", {
      allowCoenrollment: true,
      variables: {
        foo: { type: "int" },
        bar: { type: "boolean" },
        baz: {
          type: "string",
          fallbackPref,
        },
      },
    })
  );

  Services.prefs.setStringPref(fallbackPref, "pref-value");

  await manager.enroll(
    NimbusTestUtils.factories.recipe.withFeatureConfig("experiment-1", {
      branchSlug: "treatment-a",
      featureId: "foo",
      value: { foo: 1, baz: "qux" },
    }),
    "test"
  );
  await manager.enroll(
    NimbusTestUtils.factories.recipe.withFeatureConfig("experiment-2", {
      branchSlug: "treatment-b",
      featureId: "foo",
      value: { foo: 2 },
    }),
    "test"
  );
  await manager.enroll(
    NimbusTestUtils.factories.recipe.withFeatureConfig(
      "rollout-1",
      { featureId: "foo", value: { foo: 3, bar: true } },
      { isRollout: true }
    ),
    "test"
  );
  await manager.enroll(
    NimbusTestUtils.factories.recipe.withFeatureConfig(
      "rollout-2",
      { featureId: "foo", value: { bar: false, baz: "quux" } },
      { isRollout: true }
    ),
    "test"
  );

  const enrollments = NimbusFeatures.foo
    .getAllEnrollments()
    .sort((a, b) => a.meta.slug.localeCompare(b.meta.slug));

  Assert.deepEqual(
    enrollments,
    [
      {
        meta: {
          slug: "experiment-1",
          branch: "treatment-a",
          isRollout: false,
        },
        value: {
          foo: 1,
          baz: "qux",
        },
      },
      {
        meta: {
          slug: "experiment-2",
          branch: "treatment-b",
          isRollout: false,
        },
        value: {
          foo: 2,
          baz: "pref-value",
        },
      },
      {
        meta: {
          slug: "rollout-1",
          branch: "control",
          isRollout: true,
        },
        value: {
          foo: 3,
          bar: true,
          baz: "pref-value",
        },
      },
      {
        meta: {
          slug: "rollout-2",
          branch: "control",
          isRollout: true,
        },
        value: {
          bar: false,
          baz: "quux",
        },
      },
    ],
    "Should have four enrollments"
  );

  await NimbusTestUtils.cleanupManager(
    ["experiment-1", "experiment-2", "rollout-1", "rollout-2"],
    { manager }
  );
  Services.prefs.clearUserPref(fallbackPref);
  cleanupFeature();
  await cleanup();
});

add_task(async function testGetEnrollmentMetadata() {
  const { manager, cleanup } = await NimbusTestUtils.setupTest();

  const cleanupFeature = NimbusTestUtils.addTestFeatures(
    new ExperimentFeature("foo", {
      variables: {},
    })
  );

  await manager.enroll(
    NimbusTestUtils.factories.recipe.withFeatureConfig("experiment", {
      branchSlug: "treatment",
      featureId: "foo",
    }),
    "test"
  );
  await manager.enroll(
    NimbusTestUtils.factories.recipe.withFeatureConfig(
      "rollout",
      { featureId: "foo" },
      { isRollout: true }
    ),
    "test"
  );

  const enrollments = NimbusFeatures.foo
    .getAllEnrollmentMetadata()
    .sort((a, b) => a.slug.localeCompare(b.slug));

  Assert.deepEqual(
    enrollments,
    [
      {
        slug: "experiment",
        branch: "treatment",
        isRollout: false,
      },
      {
        slug: "rollout",
        branch: "control",
        isRollout: true,
      },
    ],
    "Should have two enrollments"
  );

  await NimbusTestUtils.cleanupManager(["experiment", "rollout"], { manager });

  cleanupFeature();
  await cleanup();
});

add_task(async function testGetEnrollmentMetadataCoenrolling() {
  const { manager, cleanup } = await NimbusTestUtils.setupTest();

  const cleanupFeature = NimbusTestUtils.addTestFeatures(
    new ExperimentFeature("foo", {
      allowCoenrollment: true,
      variables: {},
    })
  );

  await manager.enroll(
    NimbusTestUtils.factories.recipe.withFeatureConfig("experiment-1", {
      branchSlug: "treatment-a",
      featureId: "foo",
    }),
    "test"
  );
  await manager.enroll(
    NimbusTestUtils.factories.recipe.withFeatureConfig("experiment-2", {
      branchSlug: "treatment-b",
      featureId: "foo",
    }),
    "test"
  );
  await manager.enroll(
    NimbusTestUtils.factories.recipe.withFeatureConfig(
      "rollout-1",
      { featureId: "foo" },
      { isRollout: true }
    ),
    "test"
  );
  await manager.enroll(
    NimbusTestUtils.factories.recipe.withFeatureConfig(
      "rollout-2",
      { featureId: "foo" },
      { isRollout: true }
    ),
    "test"
  );

  const enrollments = NimbusFeatures.foo
    .getAllEnrollmentMetadata()
    .sort((a, b) => a.slug.localeCompare(b.slug));

  Assert.deepEqual(
    enrollments,
    [
      {
        slug: "experiment-1",
        branch: "treatment-a",
        isRollout: false,
      },
      {
        slug: "experiment-2",
        branch: "treatment-b",
        isRollout: false,
      },
      {
        slug: "rollout-1",
        branch: "control",
        isRollout: true,
      },
      {
        slug: "rollout-2",
        branch: "control",
        isRollout: true,
      },
    ],
    "Should have four enrollments"
  );

  await NimbusTestUtils.cleanupManager(
    ["experiment-1", "experiment-2", "rollout-1", "rollout-2"],
    { manager }
  );

  cleanupFeature();
  await cleanup();
});

add_task(async function testCoenrollingTraditionalApis() {
  const { manager, cleanup } = await NimbusTestUtils.setupTest();

  const cleanupFeature = NimbusTestUtils.addTestFeatures(
    new ExperimentFeature("foo", {
      allowCoenrollment: true,
      variables: {},
    })
  );

  await manager.enroll(
    NimbusTestUtils.factories.recipe.withFeatureConfig("experiment-1", {
      branchSlug: "treatment-a",
      featureId: "foo",
    }),
    "test"
  );
  await manager.enroll(
    NimbusTestUtils.factories.recipe.withFeatureConfig("experiment-2", {
      branchSlug: "treatment-b",
      featureId: "foo",
    }),
    "test"
  );
  await manager.enroll(
    NimbusTestUtils.factories.recipe.withFeatureConfig(
      "rollout-1",
      { featureId: "foo" },
      { isRollout: true }
    ),
    "test"
  );
  await manager.enroll(
    NimbusTestUtils.factories.recipe.withFeatureConfig(
      "rollout-2",
      { featureId: "foo" },
      { isRollout: true }
    ),
    "test"
  );

  Assert.throws(
    () => NimbusFeatures.foo.getAllVariables(),
    /Co-enrolling features must use the getAllEnrollments API/
  );

  Assert.throws(
    () => NimbusFeatures.foo.getVariable("bar"),
    /Co-enrolling features must use the getAllEnrollments API/
  );
  Assert.throws(
    () => NimbusFeatures.foo.recordExposureEvent(),
    /Co-enrolling features must provide slug/
  );

  Assert.throws(
    () => NimbusFeatures.foo.getEnrollmentMetadata(),
    /Co-enrolling features must use the getAllEnrollments or getAllEnrollmentMetadata APIs/
  );

  NimbusFeatures.foo.recordExposureEvent({ slug: "experiment-1" });
  NimbusFeatures.foo.recordExposureEvent({ slug: "rollout-2" });

  Assert.deepEqual(
    Glean.nimbusEvents.exposure.testGetValue("events")?.map(ev => ev.extra),
    [
      {
        experiment: "experiment-1",
        branch: "treatment-a",
        feature_id: "foo",
      },
      {
        experiment: "rollout-2",
        branch: "control",
        feature_id: "foo",
      },
    ]
  );

  await NimbusTestUtils.cleanupManager(
    ["experiment-1", "experiment-2", "rollout-1", "rollout-2"],
    { manager }
  );

  cleanupFeature();
  await cleanup();
});

add_task(async function testGetEnrollmentMetadata() {
  const feature = new ExperimentFeature("test-feature", {
    variables: {},
  });
  const featureId = feature.featureId;
  const cleanupFeature = NimbusTestUtils.addTestFeatures(feature);

  const { manager, cleanup } = await NimbusTestUtils.setupTest();

  const experimentMeta = {
    slug: "experiment-slug",
    branch: "treatment",
    isRollout: false,
  };

  const rolloutMeta = {
    slug: "rollout-slug",
    branch: "control",
    isRollout: true,
  };

  // There are no active enrollments.
  Assert.equal(
    NimbusFeatures[featureId].getEnrollmentMetadata("experiment"),
    null
  );
  Assert.equal(
    NimbusFeatures[featureId].getEnrollmentMetadata("rollout"),
    null
  );
  Assert.equal(NimbusFeatures[featureId].getEnrollmentMetadata(), null);

  await manager.enroll(
    NimbusTestUtils.factories.recipe.withFeatureConfig(
      "rollout-slug",
      { featureId },
      { isRollout: true }
    ),
    "test"
  );

  // Thre are no active experiments, but there is an active rollout.
  Assert.equal(
    NimbusFeatures[featureId].getEnrollmentMetadata("experiment"),
    null
  );
  Assert.deepEqual(
    NimbusFeatures[featureId].getEnrollmentMetadata("rollout"),
    rolloutMeta
  );
  Assert.deepEqual(
    NimbusFeatures[featureId].getEnrollmentMetadata(),
    rolloutMeta
  );

  await manager.enroll(
    NimbusTestUtils.factories.recipe.withFeatureConfig("experiment-slug", {
      featureId,
      branchSlug: "treatment",
    }),
    "test"
  );

  // There is an active experiment and rollout, so we should get the experiment metadata by default.
  Assert.deepEqual(
    NimbusFeatures[featureId].getEnrollmentMetadata("experiment"),
    experimentMeta
  );
  Assert.deepEqual(
    NimbusFeatures[featureId].getEnrollmentMetadata("rollout"),
    rolloutMeta
  );
  Assert.deepEqual(
    NimbusFeatures[featureId].getEnrollmentMetadata(),
    experimentMeta
  );

  await manager.unenroll("rollout-slug");

  // There is only an active experiment.
  Assert.deepEqual(
    NimbusFeatures[featureId].getEnrollmentMetadata("experiment"),
    experimentMeta
  );
  Assert.equal(
    NimbusFeatures[featureId].getEnrollmentMetadata("rollout"),
    null
  );
  Assert.deepEqual(
    NimbusFeatures[featureId].getEnrollmentMetadata(),
    experimentMeta
  );

  await manager.unenroll("experiment-slug");

  // There are no active enrollments.
  Assert.equal(
    NimbusFeatures[featureId].getEnrollmentMetadata("experiment"),
    null
  );
  Assert.equal(
    NimbusFeatures[featureId].getEnrollmentMetadata("rollout"),
    null
  );
  Assert.equal(NimbusFeatures[featureId].getEnrollmentMetadata(), null);

  await cleanup();
  cleanupFeature();
});

add_task(async function testGetEnrollmentMetadataSafe() {
  const { sandbox, manager, cleanup } = await NimbusTestUtils.setupTest();

  sandbox.stub(NimbusTelemetry, "recordExposure");
  sandbox.stub(manager.store, "getExperimentForFeature").throws();
  sandbox.stub(manager.store, "getRolloutForFeature").throws();

  Assert.equal(
    NimbusFeatures.testFeature.getEnrollmentMetadata(),
    null,
    "Should not throw"
  );
  Assert.equal(
    NimbusFeatures.testFeature.getEnrollmentMetadata("experiment"),
    null,
    "Should not throw"
  );
  Assert.equal(
    NimbusFeatures.testFeature.getEnrollmentMetadata("rollout"),
    null,
    "Should not throw"
  );

  Assert.equal(
    manager.store.getExperimentForFeature.callCount,
    2,
    "getExperimentForFeature called"
  );
  Assert.equal(
    manager.store.getRolloutForFeature.callCount,
    1,
    "getRolloutForFeature called"
  );

  NimbusFeatures.testFeature.recordExposureEvent();
  Assert.ok(
    NimbusTelemetry.recordExposure.notCalled,
    "Should not record exposure"
  );
  Assert.equal(
    manager.store.getExperimentForFeature.callCount,
    3,
    "getExperimentForFeature called"
  );

  await cleanup();
});

add_task(async function testGetProfileId() {
  const { cleanup } = await NimbusTestUtils.setupTest();

  Assert.ok(
    Services.prefs.prefHasUserValue("nimbus.profileId"),
    "nimbus.profileId set on user branch"
  );
  Assert.ok(
    !!Services.prefs.getStringPref("nimbus.profileId"),
    "can get profile ID pref"
  );

  Assert.equal(
    ExperimentAPI.profileId,
    Services.prefs.getStringPref("nimbus.profileId"),
    "ExperimentAPI.profileId matches pref value"
  );

  await cleanup();
});
