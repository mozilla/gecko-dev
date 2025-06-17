"use strict";

const { ExperimentStore } = ChromeUtils.importESModule(
  "resource://nimbus/lib/ExperimentStore.sys.mjs"
);
const { ProfilesDatastoreService } = ChromeUtils.importESModule(
  "moz-src:///toolkit/profile/ProfilesDatastoreService.sys.mjs"
);

const { SYNC_DATA_PREF_BRANCH, SYNC_DEFAULTS_PREF_BRANCH } = ExperimentStore;

add_setup(function () {
  Services.fog.initializeFOG();
});

async function setupTest({ ...args } = {}) {
  const ctx = await NimbusTestUtils.setupTest({ ...args });

  return {
    ...ctx,
    store: ctx.manager.store,
  };
}

add_task(async function test_sharedDataMap_key() {
  const store = new ExperimentStore();

  // Outside of tests we use sharedDataKey for the profile dir filepath
  // where we store experiments
  Assert.ok(store._sharedDataKey, "Make sure it's defined");
});

add_task(async function test_usageBeforeInitialization() {
  const { store, initExperimentAPI, cleanup } = await setupTest({
    init: false,
  });
  const recipe = NimbusTestUtils.factories.recipe("foo");

  Assert.equal(store.getAll().length, 0, "It should not fail");

  await initExperimentAPI();

  const experiment = NimbusTestUtils.addEnrollmentForRecipe(recipe, {
    branchSlug: "control",
  });

  Assert.equal(
    store.getExperimentForFeature("testFeature"),
    experiment,
    "should return a matching experiment for the given feature"
  );

  store.deactivateEnrollment(recipe.slug);

  await cleanup();
});

add_task(async function test_initOnUpdateEventsFire() {
  let storePath;

  {
    const store = NimbusTestUtils.stubs.store();
    await store.init();

    NimbusTestUtils.addEnrollmentForRecipe(
      NimbusTestUtils.factories.recipe.withFeatureConfig("testFeature-1", {
        featureId: "testFeature",
      }),
      { store }
    );
    NimbusTestUtils.addEnrollmentForRecipe(
      NimbusTestUtils.factories.recipe.withFeatureConfig(
        "testFeature-2",
        {
          featureId: "testFeature",
        },
        { isRollout: true }
      ),
      { store }
    );
    NimbusTestUtils.addEnrollmentForRecipe(
      NimbusTestUtils.factories.recipe.withFeatureConfig("nimbus-qa-1", {
        featureId: "nimbus-qa-1",
      }),
      {
        store,
        extra: { active: false },
      }
    );
    NimbusTestUtils.addEnrollmentForRecipe(
      NimbusTestUtils.factories.recipe.withFeatureConfig(
        "nimbus-qa-2",
        { featureId: "nimbus-qa-2" },
        { isRollout: true }
      ),
      {
        store,
        extra: { active: false },
      }
    );

    NimbusTestUtils.addEnrollmentForRecipe(
      NimbusTestUtils.factories.recipe.withFeatureConfig("coenroll-1", {
        featureId: "no-feature-firefox-desktop",
      }),
      { store }
    );
    NimbusTestUtils.addEnrollmentForRecipe(
      NimbusTestUtils.factories.recipe.withFeatureConfig("coenroll-2", {
        featureId: "no-feature-firefox-desktop",
      }),
      { store }
    );
    NimbusTestUtils.addEnrollmentForRecipe(
      NimbusTestUtils.factories.recipe.withFeatureConfig("coenroll-3", {
        featureId: "no-feature-firefox-desktop",
      }),
      { store }
    );
    NimbusTestUtils.addEnrollmentForRecipe(
      NimbusTestUtils.factories.recipe.withFeatureConfig("coenroll-4", {
        featureId: "no-feature-firefox-desktop",
      }),
      { store }
    );

    storePath = await NimbusTestUtils.saveStore(store);
  }

  const { sandbox, initExperimentAPI, cleanup } = await setupTest({
    storePath,
    init: false,
  });

  const onFeatureUpdate = sandbox.stub();

  NimbusFeatures.testFeature.onUpdate(onFeatureUpdate);
  NimbusFeatures["nimbus-qa-1"].onUpdate(onFeatureUpdate);
  NimbusFeatures["nimbus-qa-2"].onUpdate(onFeatureUpdate);
  NimbusFeatures["no-feature-firefox-desktop"].onUpdate(onFeatureUpdate);

  await initExperimentAPI();

  Assert.ok(
    onFeatureUpdate.calledWithExactly(
      "featureUpdate:testFeature",
      "feature-enrollments-loaded"
    )
  );
  Assert.ok(
    onFeatureUpdate.calledWithExactly(
      "featureUpdate:no-feature-firefox-desktop",
      "feature-enrollments-loaded"
    )
  );
  Assert.equal(
    onFeatureUpdate.callCount,
    2,
    "onFeatureUpdate called once per active feature ID"
  );

  NimbusFeatures.testFeature.offUpdate(onFeatureUpdate);

  await NimbusTestUtils.cleanupManager([
    "testFeature-1",
    "testFeature-2",
    "coenroll-1",
    "coenroll-2",
    "coenroll-3",
    "coenroll-4",
  ]);
  await cleanup();
});

add_task(async function test_getExperimentForGroup() {
  const { store, cleanup } = await setupTest();

  const experiment = NimbusTestUtils.addEnrollmentForRecipe(
    NimbusTestUtils.factories.recipe.withFeatureConfig("foo", {
      featureId: "purple",
    })
  );
  NimbusTestUtils.addEnrollmentForRecipe(
    NimbusTestUtils.factories.recipe("bar"),
    { branchSlug: "control" }
  );

  Assert.equal(
    store.getExperimentForFeature("purple"),
    experiment,
    "should return a matching experiment for the given feature"
  );

  store.deactivateEnrollment("foo");
  store.deactivateEnrollment("bar");

  await cleanup();
});

add_task(async function test_hasExperimentForFeature() {
  const { store, cleanup } = await setupTest();

  NimbusTestUtils.addEnrollmentForRecipe(
    NimbusTestUtils.factories.recipe.withFeatureConfig("foo", {
      featureId: "green",
    })
  );
  NimbusTestUtils.addEnrollmentForRecipe(
    NimbusTestUtils.factories.recipe.withFeatureConfig("foo2", {
      featureId: "yellow",
    })
  );
  NimbusTestUtils.addEnrollmentForRecipe(
    NimbusTestUtils.factories.recipe.withFeatureConfig("bar_expired", {
      featureId: "purple",
    }),
    {
      extra: { active: false },
    }
  );
  Assert.equal(
    store.hasExperimentForFeature(),
    false,
    "should return false if the input is empty"
  );

  Assert.equal(
    store.hasExperimentForFeature(undefined),
    false,
    "should return false if the input is undefined"
  );

  Assert.equal(
    store.hasExperimentForFeature("green"),
    true,
    "should return true if there is an experiment with any of the given groups"
  );

  Assert.equal(
    store.hasExperimentForFeature("purple"),
    false,
    "should return false if there is a non-active experiment with the given groups"
  );

  store.deactivateEnrollment("foo");
  store.deactivateEnrollment("foo2");

  await cleanup();
});

add_task(async function test_getAll() {
  const { store, cleanup } = await setupTest();

  NimbusTestUtils.addEnrollmentForRecipe(
    NimbusTestUtils.factories.recipe("foo"),
    { branchSlug: "control", extra: { active: false } }
  );
  NimbusTestUtils.addEnrollmentForRecipe(
    NimbusTestUtils.factories.recipe("bar"),
    { branchSlug: "control", extra: { active: false } }
  );
  NimbusTestUtils.addEnrollmentForRecipe(
    NimbusTestUtils.factories.recipe("baz"),
    { branchSlug: "control", extra: { active: false } }
  );
  NimbusTestUtils.addEnrollmentForRecipe(
    NimbusTestUtils.factories.recipe("qux"),
    { branchSlug: "control", extra: { active: true } }
  );
  NimbusTestUtils.addEnrollmentForRecipe(
    NimbusTestUtils.factories.recipe("quux", { isRollout: true }),
    { branchSlug: "control", extra: { active: false } }
  );
  NimbusTestUtils.addEnrollmentForRecipe(
    NimbusTestUtils.factories.recipe("corge", { isRollout: true }),
    { branchSlug: "control", extra: { active: false } }
  );
  NimbusTestUtils.addEnrollmentForRecipe(
    NimbusTestUtils.factories.recipe("grault", { isRollout: true }),
    { branchSlug: "control", extra: { active: false } }
  );
  NimbusTestUtils.addEnrollmentForRecipe(
    NimbusTestUtils.factories.recipe("garply", { isRollout: true }),
    { branchSlug: "control", extra: { active: true } }
  );

  Assert.deepEqual(
    store.getAll().map(e => e.slug),
    ["foo", "bar", "baz", "qux", "quux", "corge", "grault", "garply"],
    ".getAll() should return all experiments"
  );
  Assert.deepEqual(
    store.getAllActiveExperiments().map(e => e.slug),
    ["qux"],
    "getAllActiveExperiments() should return all experiments that are active"
  );
  Assert.deepEqual(
    store.getAllActiveRollouts().map(e => e.slug),
    ["garply"],
    "getAllActiveRollouts() should return all experiments that are active"
  );

  store.deactivateEnrollment("qux");
  store.deactivateEnrollment("garply");

  await cleanup();
});

add_task(async function test_addEnrollment() {
  const { store, cleanup } = await setupTest();

  const experiment = NimbusTestUtils.addEnrollmentForRecipe(
    NimbusTestUtils.factories.recipe("experiment"),
    { branchSlug: "control" }
  );
  const rollout = NimbusTestUtils.addEnrollmentForRecipe(
    NimbusTestUtils.factories.recipe("rollout", { isRollout: true })
  );

  Assert.equal(
    store.get("experiment"),
    experiment,
    "should save experiment by slug"
  );
  Assert.equal(store.get("rollout"), rollout, "should save experiment by slug");

  store.deactivateEnrollment("experiment");
  store.deactivateEnrollment("rollout");

  await cleanup();
});

add_task(async function test_deactivateEnrollment() {
  const { store, cleanup } = await setupTest();

  const enrollment = NimbusTestUtils.addEnrollmentForRecipe(
    NimbusTestUtils.factories.recipe.withFeatureConfig("foo", {
      branchSlug: "treatment",
      featureId: "no-feature-firefox-desktop",
    })
  );

  store.deactivateEnrollment("foo", "some-reason");

  Assert.deepEqual(
    store.get(enrollment.slug),
    {
      ...enrollment,
      unenrollReason: "some-reason",
      active: false,
      prefs: null,
      prefFlips: null,
    },
    "should only update relevant fields"
  );

  store.deactivateEnrollment("foo");

  await cleanup();
});

add_task(async function test_sync_access_before_init() {
  const { store, cleanup } = await setupTest();

  Assert.equal(store.getAll().length, 0, "Start with an empty store");

  NimbusTestUtils.addEnrollmentForRecipe(
    NimbusTestUtils.factories.recipe.withFeatureConfig("foo", {
      featureId: "newtab",
    })
  );

  const prefValue = JSON.parse(
    Services.prefs.getStringPref(`${SYNC_DATA_PREF_BRANCH}newtab`)
  );

  Assert.ok(prefValue, "Parsed stored experiment");
  Assert.equal(prefValue.slug, "foo", "Got back the experiment");

  // New un-initialized store that should read the pref value
  const newStore = NimbusTestUtils.stubs.store();

  Assert.equal(
    newStore.getExperimentForFeature("newtab").slug,
    "foo",
    "Returns experiment from pref"
  );

  store.deactivateEnrollment("foo");

  await cleanup();

  await NimbusTestUtils.assert.storeIsEmpty(newStore);
});

add_task(async function test_sync_features_only() {
  const { store, cleanup } = await setupTest();

  NimbusTestUtils.addEnrollmentForRecipe(
    NimbusTestUtils.factories.recipe.withFeatureConfig("foo", {
      featureId: "cfr",
    })
  );

  const newStore = NimbusTestUtils.stubs.store();
  Assert.equal(
    newStore.getAll().length,
    0,
    "cfr is not a sync access experiment"
  );

  store.deactivateEnrollment("foo");

  await cleanup();
});

add_task(async function test_sync_access_unenroll() {
  const { store, cleanup } = await setupTest();

  await store.init();

  NimbusTestUtils.addEnrollmentForRecipe(
    NimbusTestUtils.factories.recipe.withFeatureConfig("foo", {
      featureId: "aboutwelcome",
    })
  );
  store.deactivateEnrollment("foo");

  const newStore = NimbusTestUtils.stubs.store();
  Assert.equal(
    newStore.getAll().length,
    0,
    "Unenrolled experiment is not available via sync store"
  );

  await cleanup();
});

add_task(async function test_sync_access_unenroll_2() {
  const { store, cleanup } = await setupTest();

  await store.init();

  NimbusTestUtils.addEnrollmentForRecipe(
    NimbusTestUtils.factories.recipe.withFeatureConfig("foo", {
      featureId: "newtab",
    })
  );
  NimbusTestUtils.addEnrollmentForRecipe(
    NimbusTestUtils.factories.recipe.withFeatureConfig("bar", {
      featureId: "aboutwelcome",
    })
  );

  Assert.equal(store.getAll().length, 2, "2/2 experiments");

  const newStore = NimbusTestUtils.stubs.store();

  Assert.ok(
    newStore.getExperimentForFeature("aboutwelcome"),
    "Fetches experiment from pref cache even before init (aboutwelcome)"
  );

  store.deactivateEnrollment("bar");

  Assert.ok(
    newStore.getExperimentForFeature("newtab").slug,
    "Fetches experiment from pref cache even before init (newtab)"
  );
  Assert.ok(
    !newStore.getExperimentForFeature("aboutwelcome")?.slug,
    "Experiment was updated and should not be found"
  );

  store.deactivateEnrollment("foo");
  Assert.ok(
    !newStore.getExperimentForFeature("newtab")?.slug,
    "Unenrolled from 2/2 experiments"
  );

  Assert.equal(
    Services.prefs.getStringPref(`${SYNC_DATA_PREF_BRANCH}newtab`, "").length,
    0,
    "Cleared pref 1"
  );
  Assert.equal(
    Services.prefs.getStringPref(`${SYNC_DATA_PREF_BRANCH}aboutwelcome`, "")
      .length,
    0,
    "Cleared pref 2"
  );

  await cleanup();
});

add_task(async function test_getRolloutForFeature_fromStore() {
  const { store, cleanup } = await setupTest();

  await store.init();
  const rollout = NimbusTestUtils.addEnrollmentForRecipe(
    NimbusTestUtils.factories.recipe("foo", { isRollout: true })
  );

  Assert.equal(
    store.getRolloutForFeature(rollout.featureIds[0]),
    rollout,
    "Should return back the same rollout"
  );

  store.deactivateEnrollment("foo");

  await cleanup();
});

add_task(async function test_getRolloutForFeature_fromSyncCache() {
  const { store, cleanup } = await setupTest();
  const recipe = NimbusTestUtils.factories.recipe.withFeatureConfig(
    "foo",
    {
      featureId: "aboutwelcome",
      value: { enabled: true },
    },
    { isRollout: true }
  );

  NimbusTestUtils.addEnrollmentForRecipe(recipe);
  // New uninitialized store will return data from sync cache
  // before init
  const newStore = NimbusTestUtils.stubs.store();

  Assert.ok(
    Services.prefs.getStringPref(`${SYNC_DEFAULTS_PREF_BRANCH}aboutwelcome`),
    "Sync cache is set"
  );
  Assert.equal(
    newStore.getRolloutForFeature(recipe.featureIds[0]).slug,
    recipe.slug,
    "Should return back the same rollout"
  );
  Assert.deepEqual(
    newStore.getRolloutForFeature(recipe.featureIds[0]).branch.features[0],
    recipe.branches[0].features[0],
    "Should return back the same feature"
  );

  store.deactivateEnrollment(recipe.slug);

  await cleanup();
});

add_task(async function test_remoteRollout() {
  const { store, initExperimentAPI, cleanup } = await setupTest({
    init: false,
  });
  const featureUpdateStub = sinon.stub();

  const recipe = NimbusTestUtils.factories.recipe.withFeatureConfig(
    "foo",
    {
      featureId: "aboutwelcome",
      value: { enabled: true },
    },
    { isRollout: true }
  );

  store.on("featureUpdate:aboutwelcome", featureUpdateStub);

  await initExperimentAPI();

  NimbusTestUtils.addEnrollmentForRecipe(recipe);

  Assert.ok(
    Services.prefs.getStringPref(`${SYNC_DEFAULTS_PREF_BRANCH}aboutwelcome`),
    "Sync cache is set"
  );

  store.deactivateEnrollment(recipe.slug);

  Assert.ok(featureUpdateStub.calledTwice, "Called for add and remove");
  Assert.ok(
    store.get(recipe.slug),
    "Rollout is still in the store just not active"
  );
  Assert.ok(
    !store.getRolloutForFeature("aboutwelcome"),
    "Feature rollout should not exist"
  );
  Assert.ok(
    !Services.prefs.getStringPref(
      `${SYNC_DEFAULTS_PREF_BRANCH}aboutwelcome`,
      ""
    ),
    "Sync cache is cleared"
  );

  await cleanup();
});

add_task(async function test_syncDataStore_setDefault() {
  const { store, cleanup } = await setupTest();

  Assert.equal(
    Services.prefs.getStringPref(
      `${SYNC_DEFAULTS_PREF_BRANCH}aboutwelcome`,
      ""
    ),
    "",
    "Pref is empty"
  );

  NimbusTestUtils.addEnrollmentForRecipe(
    NimbusTestUtils.factories.recipe.withFeatureConfig(
      "foo",
      {
        featureId: "aboutwelcome",
        value: { remote: true },
      },
      { isRollout: true }
    )
  );

  Assert.ok(
    Services.prefs.getStringPref(`${SYNC_DEFAULTS_PREF_BRANCH}aboutwelcome`),
    "Stored in pref"
  );

  store.deactivateEnrollment("foo");

  await cleanup();
});

add_task(async function test_syncDataStore_getDefault() {
  const { store, cleanup } = await setupTest();

  const rollout = await NimbusTestUtils.addEnrollmentForRecipe(
    NimbusTestUtils.factories.recipe.withFeatureConfig(
      "aboutwelcome-slug",
      { featureId: "aboutwelcome", value: { remote: true } },
      { isRollout: true }
    )
  );

  Assert.ok(
    Services.prefs.getStringPref(`${SYNC_DEFAULTS_PREF_BRANCH}aboutwelcome`)
  );

  const restoredRollout = store.getRolloutForFeature("aboutwelcome");

  Assert.ok(restoredRollout);
  Assert.ok(
    restoredRollout.branch.features[0].value.remote,
    "Restore data from pref"
  );

  store.deactivateEnrollment(rollout.slug);

  await cleanup();
});

add_task(async function test_addEnrollment_rollout() {
  const { sandbox, store, initExperimentAPI, cleanup } = await setupTest({
    init: false,
  });

  const stub = sandbox.stub();
  const value = { bar: true };
  const recipe = NimbusTestUtils.factories.recipe.withFeatureConfig(
    "foo",
    {
      featureId: "aboutwelcome",
      value,
    },
    { isRollout: true }
  );

  store._onFeatureUpdate("aboutwelcome", stub);

  await initExperimentAPI();

  NimbusTestUtils.addEnrollmentForRecipe(recipe);

  Assert.ok(
    store.getRolloutForFeature("aboutwelcome"),
    "should return an enrollment"
  );
  Assert.equal(stub.callCount, 1, "Called once on update");
  Assert.equal(
    stub.firstCall.args[1],
    "rollout-updated",
    "Called for correct reason"
  );

  store.deactivateEnrollment("foo");

  await cleanup();
});

add_task(async function test_storeValuePerPref_returnsSameValue_allTypes() {
  const cleanupFeature = NimbusTestUtils.addTestFeatures(
    new ExperimentFeature("purple", {
      isEarlyStartup: true,
      variables: {
        string: { type: "string" },
        bool: { type: "boolean" },
        array: { type: "json" },
        number1: { type: "int" },
        number2: { type: "int" },
        number3: { type: "int" },
        json: { type: "json" },
      },
    })
  );

  const { store, cleanup } = await setupTest();

  const recipe = NimbusTestUtils.factories.recipe.withFeatureConfig("foo", {
    // Ensure it gets saved to prefs
    featureId: "purple",
    value: {
      string: "string",
      bool: true,
      array: [1, 2, 3],
      number1: 42,
      number2: 0,
      number3: -5,
      json: { jsonValue: true },
    },
  });

  NimbusTestUtils.addEnrollmentForRecipe(recipe);
  const branch = Services.prefs.getBranch(`${SYNC_DATA_PREF_BRANCH}purple.`);

  const newStore = NimbusTestUtils.stubs.store();
  Assert.deepEqual(
    newStore.getExperimentForFeature("purple").branch.features[0].value,
    recipe.branches[0].features[0].value,
    "Returns the same value"
  );

  // Cleanup
  store.deactivateEnrollment(recipe.slug);
  Assert.ok(
    !Services.prefs.getStringPref(`${SYNC_DATA_PREF_BRANCH}purple`, ""),
    "Experiment cleanup"
  );
  Assert.deepEqual(branch.getChildList(""), [], "Variables are also removed");

  await cleanup();
  cleanupFeature();
});

add_task(async function test_cleanupOldRecipes() {
  const store = NimbusTestUtils.stubs.store();

  await store.init({ cleanupOldRecipes: false });

  const NOW = Date.now();
  const SIX_HOURS = 6 * 3600 * 1000;
  const ONE_DAY = 4 * SIX_HOURS;
  const ONE_YEAR = 365.25 * 24 * 3600 * 1000;
  const ONE_MONTH = Math.floor(ONE_YEAR / 12);

  const active = NimbusTestUtils.factories.recipe("active-6hrs");
  const inactiveToday = NimbusTestUtils.factories.recipe("inactive-recent");
  const inactiveSixMonths = NimbusTestUtils.factories.recipe("inactive-6mo");
  const inactiveUnderTwelveMonths = NimbusTestUtils.factories.recipe(
    "inactive-under-12mo"
  );
  const inactiveOverTwelveMonths =
    NimbusTestUtils.factories.recipe("inactive-over-12mo");

  const inactiveNoLastSeen = NimbusTestUtils.factories.experiment(
    "inactive-unknown",
    {
      active: false,
      unenrollReason: "unknown",
    }
  );

  delete inactiveNoLastSeen.lastSeen;

  NimbusTestUtils.addEnrollmentForRecipe(active, {
    store,
    branchSlug: "control",
    extra: {
      lastSeen: new Date(NOW - SIX_HOURS).toJSON(),
    },
  });
  NimbusTestUtils.addEnrollmentForRecipe(inactiveToday, {
    store,
    branchSlug: "control",
    extra: {
      active: false,
      unenrollReason: "unknown",
      lastSeen: new Date(NOW - SIX_HOURS).toJSON(),
    },
  });
  NimbusTestUtils.addEnrollmentForRecipe(inactiveSixMonths, {
    store,
    branchSlug: "control",
    extra: {
      active: false,
      unenrollReason: "unknown",
      lastSeen: new Date(NOW - 6 * ONE_MONTH).toJSON(),
    },
  });
  NimbusTestUtils.addEnrollmentForRecipe(inactiveUnderTwelveMonths, {
    store,
    branchSlug: "control",
    extra: {
      active: false,
      unenrollReason: "unknown",
      lastSeen: new Date(NOW - ONE_YEAR + ONE_DAY).toJSON(),
    },
  });
  NimbusTestUtils.addEnrollmentForRecipe(inactiveOverTwelveMonths, {
    store,
    branchSlug: "control",
    extra: {
      active: false,
      unenrollReason: "unknown",
      lastSeen: new Date(NOW - ONE_YEAR - ONE_DAY).toJSON(),
    },
  });

  await NimbusTestUtils.flushStore();

  // There is a NOT NULL constraint that prevents adding this enrollment to the
  // database and addEnrollment() is stubbed to validate the enrollment so we
  // must use set() here.
  store.set(inactiveNoLastSeen.slug, inactiveNoLastSeen);

  // Insert a row belonging to another profile.
  const otherProfileId = Services.uuid.generateUUID().toString().slice(1, -1);

  const conn = await ProfilesDatastoreService.getConnection();
  await conn.execute(
    `
      INSERT INTO NimbusEnrollments VALUES(
        null,
        :profileId,
        :slug,
        :branchSlug,
        null,
        :active,
        :unenrollReason,
        :lastSeen,
        null,
        null,
        :source
      );
    `,
    {
      profileId: otherProfileId,
      slug: inactiveOverTwelveMonths.slug,
      branchSlug: inactiveOverTwelveMonths.branches[0].slug,
      active: false,
      unenrollReason: "unknown",
      lastSeen: new Date(NOW - ONE_YEAR - ONE_DAY).toJSON(),
      source: "NimbusTestUtils",
    }
  );

  store._cleanupOldRecipes();
  await NimbusTestUtils.flushStore(store);

  Assert.equal(
    store.get(inactiveOverTwelveMonths.slug),
    null,
    "Expired enrollment removed from in memory store"
  );
  Assert.equal(
    store.get(inactiveNoLastSeen.slug),
    null,
    "invalid enrollment removed from the store"
  );

  await NimbusTestUtils.assert.enrollmentExists(active.slug, { active: true });

  await NimbusTestUtils.assert.enrollmentExists(inactiveToday.slug, {
    active: false,
  });

  await NimbusTestUtils.assert.enrollmentExists(inactiveSixMonths.slug, {
    active: false,
  });
  await NimbusTestUtils.assert.enrollmentExists(
    inactiveUnderTwelveMonths.slug,
    { active: false }
  );
  await NimbusTestUtils.assert.enrollmentDoesNotExist(
    inactiveOverTwelveMonths.slug
  );

  // Rows in the other profile should not have been changed.
  await NimbusTestUtils.assert.enrollmentExists(inactiveOverTwelveMonths.slug, {
    active: false,
    profileId: otherProfileId,
  });

  store.deactivateEnrollment(active.slug);
  await NimbusTestUtils.flushStore();

  await NimbusTestUtils.assert.storeIsEmpty(store);
});

add_task(async function test_restore() {
  let storePath;
  {
    const store = NimbusTestUtils.stubs.store();
    await store.init();

    // This is explicitly testing restoring from the JSON store, so we don't set
    // the enrollments in the database.
    store.set("experiment", NimbusTestUtils.factories.experiment("experiment"));
    store.set(
      "rollout",
      NimbusTestUtils.factories.rollout("rollout", { active: true })
    );

    storePath = await NimbusTestUtils.saveStore(store);
  }

  const { store, cleanup } = await setupTest({ storePath });

  Assert.ok(store.get("experiment"));
  Assert.ok(store.get("rollout"));

  await NimbusTestUtils.cleanupManager(["experiment", "rollout"]);
  await cleanup();
});

add_task(async function test_restoreDatabaseConsistency() {
  Services.fog.testResetFOG();

  let storePath;

  {
    const store = await NimbusTestUtils.stubs.store();
    await store.init();

    const experimentRecipe = NimbusTestUtils.factories.recipe.withFeatureConfig(
      "experiment",
      { featureId: "no-feature-firefox-desktop" }
    );

    const rolloutRecipe = NimbusTestUtils.factories.recipe.withFeatureConfig(
      "rollout",
      { featureId: "no-feature-firefox-desktop" },
      { isRollout: true }
    );

    const inactiveRecipe = NimbusTestUtils.factories.recipe.withFeatureConfig(
      "inactive",
      { featureId: "no-feature-firefox-desktop" }
    );

    NimbusTestUtils.addEnrollmentForRecipe(experimentRecipe, { store });
    NimbusTestUtils.addEnrollmentForRecipe(rolloutRecipe, { store });
    NimbusTestUtils.addEnrollmentForRecipe(inactiveRecipe, {
      store,
      extra: { active: false },
    });

    storePath = await NimbusTestUtils.saveStore(store);

    // We should expect to see one successful databaseWrite event.
    const events = Glean.nimbusEvents.databaseWrite
      .testGetValue("events")
      .map(ev => ev.extra);

    Assert.deepEqual(events, [{ success: "true" }]);
  }

  // Initializing the store above will submit the event we care about. Disregard
  // any metrics previously recorded.
  Services.fog.testResetFOG();

  const { cleanup } = await NimbusTestUtils.setupTest({
    storePath,
    clearTelemetry: true,
    migrationState: NimbusTestUtils.migrationState.IMPORTED_ENROLLMENTS_TO_SQL,
  });

  const events = Glean.nimbusEvents.startupDatabaseConsistency
    .testGetValue("events")
    .map(ev => ev.extra);
  Assert.deepEqual(events, [
    {
      total_db_count: "3",
      total_store_count: "3",
      db_active_count: "2",
      store_active_count: "2",
      trigger: "startup",
    },
  ]);

  await NimbusTestUtils.cleanupManager(["rollout", "experiment"]);
  await cleanup();
});
