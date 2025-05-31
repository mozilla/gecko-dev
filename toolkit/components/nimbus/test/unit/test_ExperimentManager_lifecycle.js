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

const { ProfilesDatastoreService } = ChromeUtils.importESModule(
  "moz-src:///toolkit/profile/ProfilesDatastoreService.sys.mjs"
);

/**
 * onStartup()
 * - should set call setExperimentActive for each active experiment
 */
add_task(async function test_onStartup_setExperimentActive_called() {
  let storePath;

  {
    const store = NimbusTestUtils.stubs.store();
    await store.init();

    store.addEnrollment(NimbusTestUtils.factories.experiment("foo"));
    store.addEnrollment(NimbusTestUtils.factories.rollout("bar"));
    store.addEnrollment(
      NimbusTestUtils.factories.experiment("baz", { active: false })
    );
    store.addEnrollment(
      NimbusTestUtils.factories.rollout("qux", { active: false })
    );

    storePath = await NimbusTestUtils.saveStore(store);
  }

  const { sandbox, manager, initExperimentAPI, cleanup } =
    await NimbusTestUtils.setupTest({ storePath, init: false });

  sandbox.stub(NimbusTelemetry, "setExperimentActive");

  await initExperimentAPI();

  Assert.ok(
    NimbusTelemetry.setExperimentActive.calledWith(sinon.match({ slug: "foo" }))
  );
  Assert.ok(
    NimbusTelemetry.setExperimentActive.calledWith(sinon.match({ slug: "bar" }))
  );
  Assert.ok(
    !NimbusTelemetry.setExperimentActive.calledWith(
      sinon.match({ slug: "baz" })
    )
  );
  Assert.ok(
    !NimbusTelemetry.setExperimentActive.calledWith(
      sinon.match({ slug: "qux" })
    )
  );

  await manager.unenroll("foo");
  await manager.unenroll("bar");

  await cleanup();
});

add_task(async function test_startup_unenroll() {
  Services.prefs.setBoolPref("app.shield.optoutstudies.enabled", false);

  let storePath;
  {
    const store = NimbusTestUtils.stubs.store();
    await store.init();

    store.addEnrollment(
      NimbusTestUtils.factories.experiment("startup_unenroll")
    );

    storePath = await NimbusTestUtils.saveStore(store);
  }

  const { sandbox, manager, initExperimentAPI, cleanup } =
    await NimbusTestUtils.setupTest({ storePath, init: false });

  sandbox.spy(manager, "_unenroll");

  await initExperimentAPI();

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

  await cleanup();
});

add_task(async function test_onRecipe_enroll() {
  const { sandbox, manager, cleanup } = await NimbusTestUtils.setupTest();

  sandbox.stub(manager, "isInBucketAllocation").resolves(true);
  sandbox.stub(Sampling, "bucketSample").resolves(true);
  sandbox.spy(manager, "enroll");
  sandbox.spy(manager, "updateEnrollment");

  const recipe = NimbusTestUtils.factories.recipe("foo");

  Assert.deepEqual(
    manager.store.getAllActiveExperiments(),
    [],
    "There should be no active experiments"
  );

  await manager.onRecipe(recipe, "test", {
    ok: true,
    status: MatchStatus.TARGETING_AND_BUCKETING,
  });

  Assert.equal(
    manager.enroll.calledWith(recipe),
    true,
    "should call .enroll() the first time a recipe is seen"
  );
  Assert.equal(
    manager.store.has("foo"),
    true,
    "should add recipe to the store"
  );

  await manager.unenroll(recipe.slug);

  await cleanup();
});

add_task(async function test_onRecipe_update() {
  const { sandbox, manager, cleanup } = await NimbusTestUtils.setupTest();

  sandbox.spy(manager, "enroll");
  sandbox.spy(manager, "updateEnrollment");

  const recipe = NimbusTestUtils.factories.recipe("foo");

  await manager.store.init();
  await manager.onStartup();
  await manager.enroll(recipe, "test");
  await manager.onRecipe(recipe, "test", {
    ok: true,
    status: MatchStatus.TARGETING_AND_BUCKETING,
  });

  Assert.equal(
    manager.updateEnrollment.calledWith(
      sinon.match({ slug: recipe.slug }),
      recipe,
      "test",
      {
        ok: true,
        status: MatchStatus.TARGETING_AND_BUCKETING,
      }
    ),
    true,
    "should call .updateEnrollment() if the recipe has already been enrolled"
  );

  await manager.unenroll(recipe.slug);

  await cleanup();
});

add_task(async function test_onRecipe_rollout_update() {
  const { sandbox, manager, cleanup } = await NimbusTestUtils.setupTest();

  sandbox.spy(manager, "enroll");
  sandbox.spy(manager, "_unenroll");
  sandbox.spy(manager, "updateEnrollment");

  const recipe = NimbusTestUtils.factories.recipe("foo", { isRollout: true });

  await manager.enroll(recipe, "test");
  await manager.onRecipe(recipe, "test", {
    ok: true,
    status: MatchStatus.TARGETING_AND_BUCKETING,
  });

  Assert.ok(
    manager.updateEnrollment.calledOnceWith(
      sinon.match({ slug: recipe.slug }),
      recipe,
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

  const updatedRecipe = NimbusTestUtils.factories.recipe(recipe.slug, {
    isRollout: true,
    branches: [
      {
        ...recipe.branches[0],
        slug: "control-v2",
      },
    ],
  });
  await manager.onRecipe(updatedRecipe, "test", {
    ok: true,
    status: MatchStatus.TARGETING_AND_BUCKETING,
  });

  Assert.ok(
    manager.updateEnrollment.calledOnceWith(
      sinon.match({ slug: recipe.slug }),
      updatedRecipe,
      "test",
      { ok: true, status: MatchStatus.TARGETING_AND_BUCKETING }
    ),
    "should call .updateEnrollment() if the recipe has already been enrolled"
  );
  Assert.ok(
    manager._unenroll.calledOnceWith(sinon.match({ slug: recipe.slug }), {
      reason: "branch-removed",
    }),
    "updateEnrollment will unenroll because the branch slug changed"
  );

  await cleanup();
});

add_task(async function test_onRecipe_isFirefoxLabsOptin_recipe() {
  const { sandbox, manager, cleanup } = await NimbusTestUtils.setupTest();

  sandbox.stub(manager, "enroll");

  const optInRecipe = NimbusTestUtils.factories.recipe("opt-in", {
    isFirefoxLabsOptIn: true,
    firefoxLabsTitle: "title",
    firefoxLabsDescription: "description",
    firefoxLabsDescriptionLinks: null,
    firefoxLabsGroup: "group",
    requiresRestart: false,
  });
  const recipe = NimbusTestUtils.factories.recipe("recipe");

  await manager.onRecipe(optInRecipe, "test", {
    ok: true,
    status: MatchStatus.TARGETING_AND_BUCKETING,
  });
  await manager.onRecipe(recipe, "test", {
    ok: true,
    status: MatchStatus.TARGETING_AND_BUCKETING,
  });

  Assert.equal(
    manager.optInRecipes.length,
    1,
    "should only have one opt-in recipe"
  );
  Assert.equal(
    manager.optInRecipes[0],
    optInRecipe,
    "should add the recipe to OptInRecipes list if recipe is firefox labs opt-in"
  );
  Assert.equal(
    manager.enroll.calledOnceWith(recipe, "test"),
    true,
    "should try to enroll the fxLabsOptOutRecipe since it is a targetting match"
  );

  await cleanup();
});

add_task(async function test_context_paramters() {
  const { manager, cleanup } = await NimbusTestUtils.setupTest();

  const experiment = NimbusTestUtils.factories.recipe("experiment");
  const rollout = NimbusTestUtils.factories.recipe("rollout", {
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

  await manager.unenroll(experiment.slug);
  await manager.unenroll(rollout.slug);

  targetingCtx = manager.createTargetingContext();
  Assert.deepEqual(await targetingCtx.activeExperiments, []);
  Assert.deepEqual(await targetingCtx.activeRollouts, []);
  Assert.deepEqual(await targetingCtx.previousExperiments, ["experiment"]);
  Assert.deepEqual(await targetingCtx.previousRollouts, ["rollout"]);
  Assert.deepEqual([...(await targetingCtx.enrollments)].sort(), [
    "experiment",
    "rollout",
  ]);

  await cleanup();
});

add_task(async function test_experimentStore_updateEvent() {
  const { sandbox, manager, cleanup } = await NimbusTestUtils.setupTest();
  const stub = sandbox.stub();

  manager.store.on("update", stub);

  await manager.enroll(
    NimbusTestUtils.factories.recipe("experiment"),
    "rs-loader"
  );
  Assert.ok(
    stub.calledOnceWith("update", { slug: "experiment", active: true })
  );
  stub.resetHistory();

  await manager.unenroll(
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

  await cleanup();
});

add_task(async function testDb() {
  const conn = await ProfilesDatastoreService.getConnection();

  function processRow(row) {
    const fields = [
      "profileId",
      "slug",
      "branchSlug",
      "recipe",
      "active",
      "unenrollReason",
      "lastSeen",
      "setPrefs",
      "prefFlips",
      "source",
    ];

    const processed = {};

    for (const field of fields) {
      processed[field] = row.getResultByName(field);
    }

    processed.recipe = JSON.parse(processed.recipe);
    processed.setPrefs = JSON.parse(processed.setPrefs);
    processed.prefFlips = JSON.parse(processed.prefFlips);

    return processed;
  }

  async function getEnrollment(slug) {
    const results = await conn.execute(
      `
      SELECT
        profileId,
        slug,
        branchSlug,
        json(recipe) AS recipe,
        active,
        unenrollReason,
        lastSeen,
        json(setPrefs) AS setPrefs,
        json(prefFlips) AS prefFlips,
        source
      FROM NimbusEnrollments
      WHERE
        slug = :slug AND
        profileId = :profileId;
    `,
      { slug, profileId: ExperimentAPI.profileId }
    );

    Assert.equal(
      results.length,
      1,
      `Exactly one enrollment should be returned for ${slug}`
    );
    return processRow(results[0]);
  }

  async function getEnrollmentSlugs() {
    const result = await conn.execute(
      `
      SELECT
        slug
      FROM NimbusEnrollments
      WHERE
        profileId = :profileId;
    `,
      { profileId: ExperimentAPI.profileId }
    );

    return result.map(row => row.getResultByName("slug")).sort();
  }

  const { manager, cleanup } = await NimbusTestUtils.setupTest();

  const experimentRecipe = NimbusTestUtils.factories.recipe("experiment", {
    branches: [
      {
        ratio: 1,
        slug: "control",
        features: [
          {
            featureId: "no-feature-firefox-desktop",
            value: {},
          },
        ],
      },
      {
        ratio: 0, // Force enrollment in control
        slug: "treatment",
        features: [
          {
            featureId: "no-feature-firefox-desktop",
            value: {},
          },
        ],
      },
    ],
  });

  const rolloutRecipe = NimbusTestUtils.factories.recipe.withFeatureConfig(
    "rollout",
    { branchSlug: "rollout", featureId: "no-feature-firefox-desktop" }
  );

  Assert.deepEqual(
    await getEnrollmentSlugs(),
    [],
    "There are no database entries"
  );

  // Enroll in an experiment
  await manager.enroll(experimentRecipe, "test");
  Assert.deepEqual(
    await getEnrollmentSlugs(),
    [experimentRecipe.slug],
    "There is one enrollment"
  );

  let experimentEnrollment = await getEnrollment(experimentRecipe.slug);
  Assert.ok(experimentEnrollment.active, "experiment enrollment is active");
  Assert.deepEqual(
    experimentEnrollment.recipe,
    experimentRecipe,
    "experiment enrollment has the correct recipe"
  );
  Assert.equal(
    experimentEnrollment.branchSlug,
    manager.store.get(experimentRecipe.slug).branch.slug,
    "experiment branch slug matches"
  );

  // Enroll in a rollout.
  await manager.enroll(rolloutRecipe, "test");
  Assert.deepEqual(
    await getEnrollmentSlugs(),
    [experimentRecipe.slug, rolloutRecipe.slug].sort(),
    "There are two enrollments"
  );

  let rolloutEnrollment = await getEnrollment(rolloutRecipe.slug);
  Assert.ok(rolloutEnrollment.active, "rollout enrollment is active");
  Assert.deepEqual(
    rolloutEnrollment.recipe,
    rolloutRecipe,
    "rollout enrollment has the correct recipe"
  );
  Assert.equal(
    rolloutEnrollment.branchSlug,
    manager.store.get(rolloutRecipe.slug).branch.slug,
    "rollout branch slug matches"
  );

  // Unenroll from the rollout.
  await manager.unenroll(rolloutRecipe.slug, { reason: "recipe-not-seen" });
  Assert.deepEqual(
    await getEnrollmentSlugs(),
    [experimentRecipe.slug, rolloutRecipe.slug].sort(),
    "There are two enrollments"
  );

  rolloutEnrollment = await getEnrollment(rolloutRecipe.slug);
  Assert.ok(!rolloutEnrollment.active, "rollout enrollment is inactive");
  Assert.equal(
    rolloutEnrollment.recipe,
    null,
    "rollout enrollment recipe is null"
  );
  Assert.equal(
    rolloutEnrollment.unenrollReason,
    "recipe-not-seen",
    "rollout unenrollReason"
  );
  Assert.equal(
    rolloutEnrollment.branchSlug,
    manager.store.get(rolloutRecipe.slug).branch.slug,
    "rollout branch slug matches"
  );

  // Unenroll from the experiment.
  await manager.unenroll(experimentEnrollment.slug, { reason: "targeting" });

  experimentEnrollment = await getEnrollment(experimentRecipe.slug);
  Assert.ok(!experimentEnrollment.active, "experiment enrollment is inactive");
  Assert.equal(
    experimentEnrollment.recipe,
    null,
    "experiment enrollment recipe is null"
  );
  Assert.equal(
    experimentEnrollment.unenrollReason,
    "targeting",
    "experiment unenrollReason"
  );
  Assert.equal(
    experimentEnrollment.branchSlug,
    manager.store.get(experimentRecipe.slug).branch.slug,
    "experiment branch slug matches"
  );

  await cleanup();
});
