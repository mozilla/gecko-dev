"use strict";

const { NimbusTelemetry } = ChromeUtils.importESModule(
  "resource://nimbus/lib/Telemetry.sys.mjs"
);
const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);
const COLLECTION_ID_PREF = "messaging-system.rsexperimentloader.collection_id";

add_task(async function test_getExperimentMetaData() {
  const { sandbox, manager, cleanup } = await NimbusTestUtils.setupTest();

  const expected = ExperimentFakes.experiment("foo");
  let exposureStub = sandbox.stub(NimbusTelemetry, "recordExposure");

  manager.store.addEnrollment(expected);

  let metadata = ExperimentAPI.getExperimentMetaData({ slug: expected.slug });

  Assert.equal(
    Object.keys(metadata.branch).length,
    1,
    "Should only expose one property"
  );
  Assert.equal(
    metadata.branch.slug,
    expected.branch.slug,
    "Should have the slug prop"
  );

  Assert.ok(exposureStub.notCalled, "Not called for this method");

  manager.unenroll(expected.slug);
  cleanup();
});

add_task(async function test_getRolloutMetaData() {
  const { sandbox, manager, cleanup } = await NimbusTestUtils.setupTest();

  const expected = ExperimentFakes.rollout("foo");
  let exposureStub = sandbox.stub(NimbusTelemetry, "recordExposure");

  manager.store.addEnrollment(expected);

  let metadata = ExperimentAPI.getExperimentMetaData({ slug: expected.slug });

  Assert.equal(
    Object.keys(metadata.branch).length,
    1,
    "Should only expose one property"
  );
  Assert.equal(
    metadata.branch.slug,
    expected.branch.slug,
    "Should have the slug prop"
  );

  Assert.ok(exposureStub.notCalled, "Not called for this method");

  manager.unenroll(expected.slug);
  cleanup();
});

add_task(async function test_getExperimentMetaData_safe() {
  const { sandbox, manager, cleanup } = await NimbusTestUtils.setupTest();

  const exposureStub = sandbox.stub(NimbusTelemetry, "recordExposure");
  sandbox.stub(manager.store, "get").throws();
  sandbox.stub(manager.store, "getExperimentForFeature").throws();

  try {
    const metadata = ExperimentAPI.getExperimentMetaData({ slug: "foo" });
    Assert.equal(metadata, null, "Should not throw");
  } catch (e) {
    Assert.ok(false, "Error should be caught in ExperimentAPI");
  }

  Assert.ok(manager.store.get.calledOnce);

  try {
    const metadata = ExperimentAPI.getExperimentMetaData({ featureId: "foo" });
    Assert.equal(metadata, null, "Should not throw");
  } catch (e) {
    Assert.ok(false, "Error should be caught in ExperimentAPI");
  }

  Assert.ok(manager.store.getExperimentForFeature.calledOnce);
  Assert.ok(exposureStub.notCalled, "Not called for this feature");

  cleanup();
});

/**
 * #getRecipe
 */
add_task(async function test_getRecipe() {
  const { sandbox, cleanup } = await NimbusTestUtils.setupTest();
  const RECIPE = ExperimentFakes.recipe("foo");
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

  cleanup();
});

add_task(async function test_getRecipe_Failure() {
  const { sandbox, cleanup } = await NimbusTestUtils.setupTest();
  sandbox.stub(ExperimentAPI._remoteSettingsClient, "get").throws();

  const recipe = await ExperimentAPI.getRecipe("foo");
  Assert.equal(recipe, undefined, "should return undefined if RS throws");

  cleanup();
});

/**
 * #getAllBranches
 */
add_task(async function test_getAllBranches() {
  const { sandbox, cleanup } = await NimbusTestUtils.setupTest();
  const RECIPE = ExperimentFakes.recipe("foo");
  sandbox.stub(ExperimentAPI._remoteSettingsClient, "get").resolves([RECIPE]);

  const branches = await ExperimentAPI.getAllBranches("foo");
  Assert.deepEqual(
    branches,
    RECIPE.branches,
    "should return all branches if found a recipe"
  );

  cleanup();
});

// API used by Messaging System
add_task(async function test_getAllBranches_featureIdAccessor() {
  const { sandbox, cleanup } = await NimbusTestUtils.setupTest();

  const RECIPE = ExperimentFakes.recipe("foo");
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

  cleanup();
});

// For schema version before 1.6.2 branch.feature was accessed
// instead of branch.features
add_task(async function test_getAllBranches_backwardsCompat() {
  const { sandbox, cleanup } = await NimbusTestUtils.setupTest();

  const RECIPE = ExperimentFakes.recipe("foo");
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

  cleanup();
});

add_task(async function test_getAllBranches_Failure() {
  const { sandbox, cleanup } = await NimbusTestUtils.setupTest();

  sandbox.stub(ExperimentAPI._remoteSettingsClient, "get").throws();

  const branches = await ExperimentAPI.getAllBranches("foo");
  Assert.equal(branches, undefined, "should return undefined if RS throws");

  cleanup();
});

/**
 * Store events
 */
add_task(async function test_addEnrollment_eventEmit_add() {
  const { sandbox, manager, cleanup } = await NimbusTestUtils.setupTest();
  const store = manager.store;

  const featureStub = sandbox.stub();
  const experiment = ExperimentFakes.experiment("foo", {
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

  manager.unenroll(experiment.slug);
  cleanup();
});

add_task(async function test_updateExperiment_eventEmit_add_and_update() {
  const { sandbox, manager, cleanup } = await NimbusTestUtils.setupTest();
  const store = manager.store;

  const featureStub = sandbox.stub();
  const experiment = ExperimentFakes.experiment("foo", {
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

  manager.unenroll(experiment.slug);
  cleanup();
});

add_task(async function test_updateExperiment_eventEmit_off() {
  const { manager, sandbox, cleanup } = await NimbusTestUtils.setupTest();
  const store = manager.store;

  const featureStub = sandbox.stub();
  const experiment = ExperimentFakes.experiment("foo", {
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

  manager.unenroll(experiment.slug);
  cleanup();
});

add_task(async function test_getActiveBranch() {
  const { manager, cleanup } = await NimbusTestUtils.setupTest();
  const store = manager.store;

  const experiment = ExperimentFakes.experiment("foo", {
    branch: {
      slug: "variant",
      ratio: 1,
      features: [{ featureId: "green", value: {} }],
    },
  });

  store.addEnrollment(experiment);

  Assert.deepEqual(
    ExperimentAPI.getActiveBranch({ featureId: "green" }),
    experiment.branch,
    "Should return feature of active experiment"
  );

  manager.unenroll(experiment.slug);
  cleanup();
});

add_task(async function test_getActiveBranch_safe() {
  const { sandbox, manager, cleanup } = await NimbusTestUtils.setupTest();

  sandbox.stub(manager.store, "getAllActiveExperiments").throws();

  try {
    Assert.equal(
      ExperimentAPI.getActiveBranch({ featureId: "green" }),
      null,
      "Should not throw"
    );
  } catch (e) {
    Assert.ok(false, "Should catch error in ExperimentAPI");
  }

  cleanup();
});

add_task(async function test_getActiveBranch_storeFailure() {
  const { sandbox, manager, cleanup } = await NimbusTestUtils.setupTest();
  const store = manager.store;

  const experiment = ExperimentFakes.experiment("foo", {
    branch: {
      slug: "variant",
      ratio: 1,
      features: [{ featureId: "green", value: {} }],
    },
  });

  store.addEnrollment(experiment);
  // Adding stub later because `addEnrollment` emits update events
  const stub = sandbox.stub(store, "emit");
  // Call getActiveBranch to trigger an activation event
  sandbox.stub(store, "getAllActiveExperiments").throws();
  try {
    ExperimentAPI.getActiveBranch({ featureId: "green" });
  } catch (e) {
    /* This is expected */
  }

  Assert.equal(stub.callCount, 0, "Not called if store somehow fails");

  manager.unenroll(experiment.slug);
  cleanup();
});

add_task(async function test_getActiveBranch_noActivationEvent() {
  const { sandbox, manager, cleanup } = await NimbusTestUtils.setupTest();
  const store = manager.store;

  const experiment = ExperimentFakes.experiment("foo", {
    branch: {
      slug: "variant",
      ratio: 1,
      features: [{ featureId: "green", value: {} }],
    },
  });

  store.addEnrollment(experiment);
  // Adding stub later because `addEnrollment` emits update events
  const stub = sandbox.stub(store, "emit");
  // Call getActiveBranch to trigger an activation event
  ExperimentAPI.getActiveBranch({ featureId: "green" });

  Assert.equal(stub.callCount, 0, "Not called: sendExposureEvent is false");

  manager.unenroll(experiment.slug);
  cleanup();
});
