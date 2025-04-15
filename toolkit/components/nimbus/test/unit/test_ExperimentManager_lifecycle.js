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
const { UnenrollmentCause } = ChromeUtils.importESModule(
  "resource://nimbus/lib/ExperimentManager.sys.mjs"
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
  sandbox.spy(manager, "_unenroll");

  await manager.onStartup();

  Assert.ok(
    manager._unenroll.calledOnceWith(
      sinon.match({ slug: "startup_unenroll" }),
      {
        reason: "studies-opt-out",
      }
    ),
    "Called unenroll for expected recipe"
  );

  Services.prefs.clearUserPref("app.shield.optoutstudies.enabled");

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

  await manager.onRecipe(fooRecipe, "test", {
    ok: true,
    status: MatchStatus.TARGETING_AND_BUCKETING,
  });

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

  manager.unenroll(fooRecipe.slug);

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
  await manager.onRecipe(fooRecipe, "test", {
    ok: true,
    status: MatchStatus.TARGETING_AND_BUCKETING,
  });

  Assert.equal(
    manager.updateEnrollment.calledWith(sinon.match.object, fooRecipe, "test", {
      ok: true,
      status: MatchStatus.TARGETING_AND_BUCKETING,
    }),
    true,
    "should call .updateEnrollment() if the recipe has already been enrolled"
  );

  manager.unenroll(fooRecipe.slug);

  await cleanupStore(manager.store);
});

add_task(async function test_onRecipe_rollout_update() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  sandbox.spy(manager, "enroll");
  sandbox.spy(manager, "_unenroll");
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
  await manager.onRecipe(fooRecipe, "test", {
    ok: true,
    status: MatchStatus.TARGETING_AND_BUCKETING,
  });

  Assert.ok(
    manager.updateEnrollment.calledOnceWith(
      sinon.match.object,
      fooRecipe,
      "test",
      { ok: true, status: MatchStatus.TARGETING_AND_BUCKETING }
    ),
    "should call .updateEnrollment() if the recipe has already been enrolled"
  );
  Assert.ok(
    manager.updateEnrollment.alwaysReturned(Promise.resolve(true)),
    "updateEnrollment will confirm the enrolled branch still exists in the recipe and exit"
  );
  Assert.ok(
    manager._unenroll.notCalled,
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
  await manager.onRecipe(recipeClone, "test", {
    ok: true,
    status: MatchStatus.TARGETING_AND_BUCKETING,
  });

  Assert.ok(
    manager.updateEnrollment.calledOnceWith(
      sinon.match.object,
      recipeClone,
      "test",
      { ok: true, status: MatchStatus.TARGETING_AND_BUCKETING }
    ),
    "should call .updateEnrollment() if the recipe has already been enrolled"
  );
  Assert.ok(
    manager._unenroll.calledOnceWith(sinon.match({ slug: fooRecipe.slug }), {
      reason: "branch-removed",
    }),
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

  await manager.onRecipe(fxLabsOptInRecipe, "test", {
    ok: true,
    status: MatchStatus.TARGETING_AND_BUCKETING,
  });
  await manager.onRecipe(fxLabsOptOutRecipe, "test", {
    ok: true,
    status: MatchStatus.TARGETING_AND_BUCKETING,
  });

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

  manager.unenroll(
    "experiment",
    UnenrollmentCause.fromReason(
      NimbusTelemetry.UnenrollReason.INDIVIDUAL_OPT_OUT
    )
  );
  Assert.ok(
    stub.calledOnceWith("update", {
      slug: "experiment",
      active: false,
      unenrollReason: "individual-opt-out",
    })
  );

  assertEmptyStore(manager.store);
});
