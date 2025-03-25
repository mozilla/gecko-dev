"use strict";

const { Sampling } = ChromeUtils.importESModule(
  "resource://gre/modules/components-utils/Sampling.sys.mjs"
);

const { MatchStatus } = ChromeUtils.importESModule(
  "resource://nimbus/lib/RemoteSettingsExperimentLoader.sys.mjs"
);

const { NimbusTelemetry } = ChromeUtils.importESModule(
  "resource://nimbus/lib/Telemetry.sys.mjs"
);

async function cleanupStore(store) {
  Assert.deepEqual(
    store.getAllActiveExperiments(),
    [],
    "There should be no experiments active."
  );

  Assert.deepEqual(
    store.getAllActiveRollouts(),
    [],
    "There should be no rollouts active"
  );

  // We need to call finalize first to ensure that any pending saves from
  // JSONFile.saveSoon overwrite files on disk.
  await store._store.finalize();
  await IOUtils.remove(store._store.path);
}

/**
 * onStartup()
 * - should set call setExperimentActive for each active experiment
 */
add_task(async function test_onStartup_setExperimentActive_called() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  const experiments = [];
  sandbox.stub(NimbusTelemetry, "setExperimentActive");
  sandbox.stub(manager.store, "init").resolves();
  sandbox.stub(manager.store, "getAll").returns(experiments);
  sandbox
    .stub(manager.store, "get")
    .callsFake(slug => experiments.find(expt => expt.slug === slug));
  sandbox.stub(manager.store, "set");

  const active = ["foo", "bar"].map(ExperimentFakes.experiment);

  const inactive = ["baz", "qux"].map(slug =>
    ExperimentFakes.experiment(slug, { active: false })
  );

  [...active, ...inactive].forEach(exp => experiments.push(exp));

  await manager.onStartup();

  active.forEach(exp =>
    Assert.equal(
      NimbusTelemetry.setExperimentActive.calledWith(exp),
      true,
      `should call setExperimentActive for active experiment: ${exp.slug}`
    )
  );

  inactive.forEach(exp =>
    Assert.equal(
      NimbusTelemetry.setExperimentActive.calledWith(exp),
      false,
      `should not call setExperimentActive for inactive experiment: ${exp.slug}`
    )
  );

  sandbox.restore();
  await cleanupStore(manager.store);
});

add_task(async function test_onStartup_setRolloutActive_called() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  sandbox.stub(NimbusTelemetry, "setExperimentActive");
  sandbox.stub(manager.store, "init").resolves();

  const active = ["foo", "bar"].map(ExperimentFakes.rollout);
  sandbox.stub(manager.store, "getAll").returns(active);
  sandbox
    .stub(manager.store, "get")
    .callsFake(slug => active.find(e => e.slug === slug));
  sandbox.stub(manager.store, "set");

  await manager.onStartup();

  active.forEach(r =>
    Assert.equal(
      NimbusTelemetry.setExperimentActive.calledWith(r),
      true,
      `should call setExperimentActive for rollout: ${r.slug}`
    )
  );

  sandbox.restore();
  await cleanupStore(manager.store);
});

add_task(async function test_startup_unenroll() {
  Services.prefs.setBoolPref("app.shield.optoutstudies.enabled", false);
  const store = ExperimentFakes.store();
  const sandbox = sinon.createSandbox();
  let recipe = ExperimentFakes.experiment("startup_unenroll", {
    experimentType: "unittest",
    source: "test",
  });
  // Test initializing ExperimentManager with an active
  // recipe in the store. If the user has opted out it should
  // unenroll.
  await store.init();
  store.addEnrollment(recipe);

  const manager = ExperimentFakes.manager(store);
  const unenrollSpy = sandbox.spy(manager, "unenroll");

  await manager.onStartup();

  Assert.ok(
    unenrollSpy.calledOnce,
    "Unenrolled from active experiment if user opt out is true"
  );
  Assert.ok(
    unenrollSpy.calledWith("startup_unenroll", "studies-opt-out"),
    "Called unenroll for expected recipe"
  );

  Services.prefs.clearUserPref("app.shield.optoutstudies.enabled");

  await cleanupStore(manager.store);
});

/**
 * onRecipe()
 * - should add recipe slug to .session[source]
 * - should call .enroll() if the recipe hasn't been seen before;
 * - should call .update() if the Enrollment already exists in the store;
 * - should skip enrollment if recipe.isEnrollmentPaused is true
 */
add_task(async function test_onRecipe_track_slug() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  sandbox.spy(manager, "enroll");
  sandbox.spy(manager, "updateEnrollment");

  const fooRecipe = ExperimentFakes.recipe("foo");

  await manager.onStartup();
  await manager.onRecipe(fooRecipe, "test", MatchStatus.TARGETING_ONLY);

  Assert.equal(
    manager.sessions.get("test").has("foo"),
    true,
    "should add slug to sessions[test]"
  );

  await cleanupStore(manager.store);
});

add_task(async function test_onRecipe_enroll() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  sandbox.stub(manager, "isInBucketAllocation").resolves(true);
  sandbox.stub(Sampling, "bucketSample").resolves(true);
  sandbox.spy(manager, "enroll");
  sandbox.spy(manager, "updateEnrollment");

  const fooRecipe = ExperimentFakes.recipe("foo");
  await manager.onStartup();

  Assert.deepEqual(
    manager.store.getAllActiveExperiments(),
    [],
    "There should be no active experiments"
  );

  await manager.onRecipe(
    fooRecipe,
    "test",
    MatchStatus.TARGETING_AND_BUCKETING
  );

  Assert.equal(
    manager.enroll.calledWith(fooRecipe),
    true,
    "should call .enroll() the first time a recipe is seen"
  );
  Assert.equal(
    manager.store.has("foo"),
    true,
    "should add recipe to the store"
  );

  manager.unenroll(fooRecipe.slug, "test-cleanup");

  await cleanupStore(manager.store);
});

add_task(async function test_onRecipe_update() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  sandbox.spy(manager, "enroll");
  sandbox.spy(manager, "updateEnrollment");

  const fooRecipe = ExperimentFakes.recipe("foo", {
    bucketConfig: {
      ...ExperimentFakes.recipe.bucketConfig,
      count: 1000,
    },
  });

  await manager.onStartup();
  await manager.enroll(fooRecipe, "test");
  await manager.onRecipe(
    fooRecipe,
    "test",
    MatchStatus.TARGETING_AND_BUCKETING
  );

  Assert.equal(
    manager.updateEnrollment.calledWith(fooRecipe),
    true,
    "should call .updateEnrollment() if the recipe has already been enrolled"
  );

  manager.unenroll(fooRecipe.slug, "test-cleanup");

  await cleanupStore(manager.store);
});

add_task(async function test_onRecipe_rollout_update() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  sandbox.spy(manager, "enroll");
  sandbox.spy(manager, "unenroll");
  sandbox.spy(manager, "updateEnrollment");

  const fooRecipe = ExperimentFakes.recipe("foo", {
    isRollout: true,
    bucketConfig: {
      ...ExperimentFakes.recipe.bucketConfig,
      count: 1000,
    },
    branches: [ExperimentFakes.recipe.branches[0]],
  });

  await manager.onStartup();
  await manager.enroll(fooRecipe, "test");
  await manager.onRecipe(
    fooRecipe,
    "test",
    MatchStatus.TARGETING_AND_BUCKETING
  );

  Assert.ok(
    manager.updateEnrollment.calledOnceWith(fooRecipe, "test", true),
    "should call .updateEnrollment() if the recipe has already been enrolled"
  );
  Assert.ok(
    manager.updateEnrollment.alwaysReturned(Promise.resolve(true)),
    "updateEnrollment will confirm the enrolled branch still exists in the recipe and exit"
  );
  Assert.ok(
    manager.unenroll.notCalled,
    "Should not call if the branches did not change"
  );

  manager.updateEnrollment.resetHistory();

  const recipeClone = ExperimentFakes.recipe("foo", {
    isRollout: true,
    branches: [
      {
        ...ExperimentFakes.recipe.branches[0],
        slug: "control-v2",
      },
    ],
  });
  await manager.onRecipe(
    recipeClone,
    "test",
    MatchStatus.TARGETING_AND_BUCKETING
  );

  Assert.ok(
    manager.updateEnrollment.calledOnceWith(recipeClone, "test", true),
    "should call .updateEnrollment() if the recipe has already been enrolled"
  );
  Assert.ok(
    manager.unenroll.called,
    "updateEnrollment will unenroll because the branch slug changed"
  );
  Assert.ok(
    manager.unenroll.calledWith(fooRecipe.slug, "branch-removed"),
    "updateEnrollment will unenroll because the branch slug changed"
  );

  await cleanupStore(manager.store);
});

add_task(async function test_onRecipe_isFirefoxLabsOptin_recipe() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  sandbox.spy(manager, "enroll");

  const fxLabsOptInRecipe = ExperimentFakes.recipe("fxLabsOptIn", {
    isFirefoxLabsOptIn: true,
    bucketConfig: {
      ...ExperimentFakes.recipe.bucketConfig,
      count: 10000,
    },
    firefoxLabsTitle: "title",
    firefoxLabsDescription: "description",
    firefoxLabsDescriptionLinks: null,
    firefoxLabsGroup: "group",
    requiresRestart: false,
  });
  const fxLabsOptOutRecipe = ExperimentFakes.recipe("fxLabsOptOut", {
    isFirefoxLabsOptIn: false,
    bucketConfig: {
      ...ExperimentFakes.recipe.bucketConfig,
      count: 1000,
    },
    firefoxLabsTitle: null,
    firefoxLabsDescription: null,
    firefoxLabsDescriptionLinks: null,
    firefoxLabsGroup: null,
    requiresRestart: false,
  });

  await manager.onStartup();

  await manager.onRecipe(
    fxLabsOptInRecipe,
    "test",
    MatchStatus.TARGETING_AND_BUCKETING
  );
  await manager.onRecipe(
    fxLabsOptOutRecipe,
    "test",
    MatchStatus.TARGETING_AND_BUCKETING
  );

  Assert.equal(
    manager.optInRecipes.length,
    1,
    "should only have one recipe i.e fxLabsOptInRecipe"
  );
  Assert.equal(
    manager.optInRecipes[0],
    fxLabsOptInRecipe,
    "should add the recipe to OptInRecipes list if recipe is firefox labs opt-in"
  );
  Assert.equal(
    manager.enroll.calledOnceWith(fxLabsOptOutRecipe, "test"),
    true,
    "should try to enroll the fxLabsOptOutRecipe since it is a targetting match"
  );

  // unenrolling the fxLabsOptOutRecipe only
  manager.unenroll(fxLabsOptOutRecipe.slug);
  await cleanupStore(manager.store);
});

/**
 * onFinalize()
 * - should unenroll experiments that weren't seen in the current session
 */

add_task(async function test_onFinalize_unenroll() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  sandbox.spy(manager, "unenroll");

  await manager.onStartup();

  // Add an experiment to the store without calling .onRecipe
  // This simulates an enrollment having happened in the past.
  let recipe0 = ExperimentFakes.experiment("foo", {
    experimentType: "unittest",
    userFacingName: "foo",
    userFacingDescription: "foo",
    lastSeen: new Date().toJSON(),
    source: "test",
  });
  await manager.store.addEnrollment(recipe0);

  const recipe1 = ExperimentFakes.recipe("bar");
  // Unique features to prevent overlap
  recipe1.branches[0].features[0].featureId = "red";
  recipe1.branches[1].features[0].featureId = "red";
  await manager.onRecipe(recipe1, "test", MatchStatus.TARGETING_AND_BUCKETING);
  const recipe2 = ExperimentFakes.recipe("baz");
  recipe2.branches[0].features[0].featureId = "green";
  recipe2.branches[1].features[0].featureId = "green";
  await manager.onRecipe(recipe2, "test", MatchStatus.TARGETING_AND_BUCKETING);

  // Finalize
  manager.onFinalize("test");

  Assert.equal(
    manager.unenroll.callCount,
    1,
    "should only call unenroll for the unseen recipe"
  );
  Assert.equal(
    manager.unenroll.calledWith("foo", "recipe-not-seen"),
    true,
    "should unenroll a experiment whose recipe wasn't seen in the current session"
  );
  Assert.equal(
    manager.sessions.has("test"),
    false,
    "should clear sessions[test]"
  );

  manager.unenroll(recipe1.slug);
  manager.unenroll(recipe2.slug);
  await cleanupStore(manager.store);
});

add_task(async function test_onFinalize_unenroll_mismatch() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  sandbox.spy(manager, "unenroll");

  await manager.onStartup();

  // Add an experiment to the store without calling .onRecipe
  // This simulates an enrollment having happened in the past.
  let recipe0 = ExperimentFakes.experiment("foo", {
    experimentType: "unittest",
    userFacingName: "foo",
    userFacingDescription: "foo",
    lastSeen: new Date().toJSON(),
    source: "test",
  });
  await manager.store.addEnrollment(recipe0);

  const recipe1 = ExperimentFakes.recipe("bar");
  // Unique features to prevent overlap
  recipe1.branches[0].features[0].featureId = "red";
  recipe1.branches[1].features[0].featureId = "red";
  await manager.onRecipe(recipe1, "test", MatchStatus.TARGETING_AND_BUCKETING);
  const recipe2 = ExperimentFakes.recipe("baz");
  recipe2.branches[0].features[0].featureId = "green";
  recipe2.branches[1].features[0].featureId = "green";
  await manager.onRecipe(recipe2, "test", MatchStatus.TARGETING_AND_BUCKETING);

  // Finalize
  manager.onFinalize("test", { recipeMismatches: [recipe0.slug] });

  Assert.equal(
    manager.unenroll.callCount,
    1,
    "should only call unenroll for the unseen recipe"
  );
  Assert.equal(
    manager.unenroll.calledWith("foo", "targeting-mismatch"),
    true,
    "should unenroll a experiment whose recipe wasn't seen in the current session"
  );
  Assert.equal(
    manager.sessions.has("test"),
    false,
    "should clear sessions[test]"
  );

  manager.unenroll(recipe1.slug);
  manager.unenroll(recipe2.slug);
  await cleanupStore(manager.store);
});

add_task(async function test_onFinalize_rollout_unenroll() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  sandbox.spy(manager, "unenroll");

  await manager.onStartup();

  let rollout = ExperimentFakes.rollout("rollout");
  await manager.store.addEnrollment(rollout);

  manager.onFinalize("NimbusTestUtils");

  Assert.equal(
    manager.unenroll.callCount,
    1,
    "should only call unenroll for the unseen recipe"
  );
  Assert.equal(
    manager.unenroll.calledWith("rollout", "recipe-not-seen"),
    true,
    "should unenroll a experiment whose recipe wasn't seen in the current session"
  );

  await cleanupStore(manager.store);
});

add_task(async function test_context_paramters() {
  const manager = ExperimentFakes.manager();

  await manager.onStartup();
  await manager.store.ready();

  const experiment = ExperimentFakes.recipe("experiment", {
    bucketConfig: {
      ...ExperimentFakes.recipe.bucketConfig,
      count: 1000,
    },
  });

  const rollout = ExperimentFakes.recipe("rollout", {
    bucketConfig: experiment.bucketConfig,
    isRollout: true,
  });

  let targetingCtx = manager.createTargetingContext();

  Assert.deepEqual(await targetingCtx.activeExperiments, []);
  Assert.deepEqual(await targetingCtx.activeRollouts, []);
  Assert.deepEqual(await targetingCtx.previousExperiments, []);
  Assert.deepEqual(await targetingCtx.previousRollouts, []);
  Assert.deepEqual(await targetingCtx.enrollments, []);

  await manager.enroll(experiment, "test");
  await manager.enroll(rollout, "test");

  targetingCtx = manager.createTargetingContext();
  Assert.deepEqual(await targetingCtx.activeExperiments, ["experiment"]);
  Assert.deepEqual(await targetingCtx.activeRollouts, ["rollout"]);
  Assert.deepEqual(await targetingCtx.previousExperiments, []);
  Assert.deepEqual(await targetingCtx.previousRollouts, []);
  Assert.deepEqual([...(await targetingCtx.enrollments)].sort(), [
    "experiment",
    "rollout",
  ]);

  manager.unenroll(experiment.slug);
  manager.unenroll(rollout.slug);

  targetingCtx = manager.createTargetingContext();
  Assert.deepEqual(await targetingCtx.activeExperiments, []);
  Assert.deepEqual(await targetingCtx.activeRollouts, []);
  Assert.deepEqual(await targetingCtx.previousExperiments, ["experiment"]);
  Assert.deepEqual(await targetingCtx.previousRollouts, ["rollout"]);
  Assert.deepEqual([...(await targetingCtx.enrollments)].sort(), [
    "experiment",
    "rollout",
  ]);
});

add_task(async function test_experimentStore_updateEvent() {
  const manager = ExperimentFakes.manager();
  const stub = sinon.stub();

  await manager.onStartup();
  await manager.store.ready();

  manager.store.on("update", stub);

  await manager.enroll(
    ExperimentFakes.recipe("experiment", {
      bucketConfig: {
        ...ExperimentFakes.recipe.bucketConfig,
        count: 1000,
      },
    }),
    "rs-loader"
  );
  Assert.ok(
    stub.calledOnceWith("update", { slug: "experiment", active: true })
  );
  stub.resetHistory();

  manager.unenroll("experiment", "individual-opt-out");
  Assert.ok(
    stub.calledOnceWith("update", {
      slug: "experiment",
      active: false,
      unenrollReason: "individual-opt-out",
    })
  );

  await assertEmptyStore(manager.store);
});
