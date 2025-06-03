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
  const experiment = NimbusTestUtils.factories.experiment("foo");

  Assert.equal(store.getAll().length, 0, "It should not fail");

  await initExperimentAPI();

  store.addEnrollment(experiment);

  Assert.equal(
    store.getExperimentForFeature("testFeature"),
    experiment,
    "should return a matching experiment for the given feature"
  );

  store.updateExperiment(experiment.slug, { active: false });

  await cleanup();
});

add_task(async function test_initOnUpdateEventsFire() {
  let storePath;

  {
    const store = NimbusTestUtils.stubs.store();
    await store.init();

    store.addEnrollment(
      NimbusTestUtils.factories.experiment.withFeatureConfig("testFeature-1", {
        featureId: "testFeature",
      })
    );
    store.addEnrollment(
      NimbusTestUtils.factories.rollout.withFeatureConfig("testFeature-2", {
        featureId: "testFeature",
      })
    );
    store.addEnrollment(
      NimbusTestUtils.factories.experiment.withFeatureConfig(
        "nimbus-qa-1",
        { featureId: "nimbus-qa-1" },
        { active: false }
      )
    );
    store.addEnrollment(
      NimbusTestUtils.factories.rollout.withFeatureConfig(
        "nimbus-qa-2",
        { featureId: "nimbus-qa-2" },
        { active: false }
      )
    );

    store.addEnrollment(
      NimbusTestUtils.factories.experiment.withFeatureConfig("coenroll-1", {
        featureId: "no-feature-firefox-desktop",
      })
    );
    store.addEnrollment(
      NimbusTestUtils.factories.experiment.withFeatureConfig("coenroll-2", {
        featureId: "no-feature-firefox-desktop",
      })
    );
    store.addEnrollment(
      NimbusTestUtils.factories.rollout.withFeatureConfig("coenroll-3", {
        featureId: "no-feature-firefox-desktop",
      })
    );
    store.addEnrollment(
      NimbusTestUtils.factories.rollout.withFeatureConfig("coenroll-4", {
        featureId: "no-feature-firefox-desktop",
      })
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

  const experiment = NimbusTestUtils.factories.experiment.withFeatureConfig(
    "foo",
    {
      featureId: "purple",
    }
  );

  store.addEnrollment(NimbusTestUtils.factories.experiment("bar"));
  store.addEnrollment(experiment);

  Assert.equal(
    store.getExperimentForFeature("purple"),
    experiment,
    "should return a matching experiment for the given feature"
  );

  store.updateExperiment("foo", { active: false });
  store.updateExperiment("bar", { active: false });

  await cleanup();
});

add_task(async function test_hasExperimentForFeature() {
  const { store, cleanup } = await setupTest();

  store.addEnrollment(
    NimbusTestUtils.factories.experiment.withFeatureConfig("foo", {
      featureId: "green",
    })
  );
  store.addEnrollment(
    NimbusTestUtils.factories.experiment.withFeatureConfig("foo2", {
      featureId: "yellow",
    })
  );
  store.addEnrollment(
    NimbusTestUtils.factories.experiment.withFeatureConfig(
      "bar_expired",
      { featureId: "purple" },
      {
        active: false,
      }
    )
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

  store.updateExperiment("foo", { active: false });
  store.updateExperiment("foo2", { active: false });

  await cleanup();
});

add_task(async function test_getAll() {
  const { store, cleanup } = await setupTest();

  store.addEnrollment(
    NimbusTestUtils.factories.experiment("foo", { active: false })
  );
  store.addEnrollment(
    NimbusTestUtils.factories.experiment("bar", { active: false })
  );
  store.addEnrollment(
    NimbusTestUtils.factories.experiment("baz", { active: false })
  );
  store.addEnrollment(
    NimbusTestUtils.factories.experiment("qux", { active: true })
  );
  store.addEnrollment(
    NimbusTestUtils.factories.rollout("quux", { active: false })
  );
  store.addEnrollment(
    NimbusTestUtils.factories.rollout("corge", { active: false })
  );
  store.addEnrollment(
    NimbusTestUtils.factories.rollout("grault", { active: false })
  );
  store.addEnrollment(
    NimbusTestUtils.factories.rollout("garply", { active: true })
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

  store.updateExperiment("qux", { active: false });
  store.updateExperiment("garply", { active: false });

  await cleanup();
});

add_task(async function test_addEnrollment() {
  const { store, cleanup } = await setupTest();

  const experiment = NimbusTestUtils.factories.experiment("experiment");
  const rollout = NimbusTestUtils.factories.experiment("rollout");

  store.addEnrollment(experiment);
  store.addEnrollment(rollout);

  Assert.equal(
    store.get("experiment"),
    experiment,
    "should save experiment by slug"
  );
  Assert.equal(store.get("rollout"), rollout, "should save experiment by slug");

  store.updateExperiment("experiment", { active: false });
  store.updateExperiment("rollout", { active: false });

  await cleanup();
});

add_task(async function test_updateExperiment() {
  const { store, cleanup } = await setupTest();

  const features = [{ featureId: "cfr", value: {} }];
  const experiment = Object.freeze(
    NimbusTestUtils.factories.experiment("foo", {
      branch: {
        slug: "treatment",
        ratio: 1,
        features,
      },
      active: true,
    })
  );

  store.addEnrollment(experiment);
  store.updateExperiment(experiment.slug, { active: false });

  const actual = store.get("foo");
  Assert.equal(actual.active, false, "should change updated props");
  Assert.deepEqual(
    actual.branch.features,
    features,
    "should not update other props"
  );

  store.updateExperiment("foo", { active: false });

  await cleanup();
});

add_task(async function test_sync_access_before_init() {
  const { store, cleanup } = await setupTest();

  Assert.equal(store.getAll().length, 0, "Start with an empty store");

  const experiment = NimbusTestUtils.factories.experiment.withFeatureConfig(
    "foo",
    {
      featureId: "newtab",
    }
  );
  store.addEnrollment(experiment);

  const prefValue = JSON.parse(
    Services.prefs.getStringPref(`${SYNC_DATA_PREF_BRANCH}newtab`)
  );

  Assert.ok(prefValue, "Parsed stored experiment");
  Assert.equal(prefValue.slug, experiment.slug, "Got back the experiment");

  // New un-initialized store that should read the pref value
  const newStore = NimbusTestUtils.stubs.store();

  Assert.equal(
    newStore.getExperimentForFeature("newtab").slug,
    "foo",
    "Returns experiment from pref"
  );

  store.updateExperiment("foo", { active: false });

  await cleanup();

  await NimbusTestUtils.assert.storeIsEmpty(newStore);
});

add_task(async function test_sync_access_update() {
  const { store, cleanup } = await setupTest();

  const experiment = NimbusTestUtils.factories.experiment.withFeatureConfig(
    "foo",
    {
      featureId: "aboutwelcome",
    }
  );

  store.addEnrollment(experiment);
  store.updateExperiment("foo", {
    branch: {
      ...experiment.branch,
      features: [
        {
          featureId: "aboutwelcome",
          value: { bar: "bar", enabled: true },
        },
      ],
    },
  });

  const newStore = NimbusTestUtils.stubs.store();
  const cachedExperiment = newStore.getExperimentForFeature("aboutwelcome");

  Assert.ok(cachedExperiment, "Got back 1 experiment");
  Assert.deepEqual(
    // `branch.feature` and not `features` because for sync access (early startup)
    // experiments we only store the `isEarlyStartup` feature
    cachedExperiment.branch.features[0].value,
    { bar: "bar", enabled: true },
    "Got updated value"
  );

  store.updateExperiment("foo", { active: false });

  await cleanup();

  await NimbusTestUtils.assert.storeIsEmpty(newStore);
});

add_task(async function test_sync_features_only() {
  const { store, cleanup } = await setupTest();

  store.addEnrollment(
    NimbusTestUtils.factories.experiment.withFeatureConfig("foo", {
      featureId: "cfr",
    })
  );

  const newStore = NimbusTestUtils.stubs.store();
  Assert.equal(
    newStore.getAll().length,
    0,
    "cfr is not a sync access experiment"
  );

  store.updateExperiment("foo", { active: false });

  await cleanup();
});

add_task(async function test_sync_access_unenroll() {
  const { store, cleanup } = await setupTest();

  let experiment = NimbusTestUtils.factories.experiment.withFeatureConfig(
    "foo",
    {
      featureId: "aboutwelcome",
    }
  );

  await store.init();

  store.addEnrollment(experiment);
  store.updateExperiment("foo", { active: false });

  const newStore = NimbusTestUtils.stubs.store();
  Assert.equal(newStore.getAll().length, 0, "Unenrolled experiment is deleted");

  await cleanup();
});

add_task(async function test_sync_access_unenroll_2() {
  const { store, cleanup } = await setupTest();

  let experiment1 = NimbusTestUtils.factories.experiment.withFeatureConfig(
    "foo",
    { featureId: "newtab" }
  );
  let experiment2 = NimbusTestUtils.factories.experiment.withFeatureConfig(
    "bar",
    { featureId: "aboutwelcome" }
  );

  await store.init();

  store.addEnrollment(experiment1);
  store.addEnrollment(experiment2);

  Assert.equal(store.getAll().length, 2, "2/2 experiments");

  const newStore = NimbusTestUtils.stubs.store();

  Assert.ok(
    newStore.getExperimentForFeature("aboutwelcome"),
    "Fetches experiment from pref cache even before init (aboutwelcome)"
  );

  store.updateExperiment("bar", { active: false });

  Assert.ok(
    newStore.getExperimentForFeature("newtab").slug,
    "Fetches experiment from pref cache even before init (newtab)"
  );
  Assert.ok(
    !newStore.getExperimentForFeature("aboutwelcome")?.slug,
    "Experiment was updated and should not be found"
  );

  store.updateExperiment("foo", { active: false });
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
  const rollout = NimbusTestUtils.factories.rollout("foo");

  await store.init();
  store.addEnrollment(rollout);

  Assert.deepEqual(
    store.getRolloutForFeature(rollout.featureIds[0]),
    rollout,
    "Should return back the same rollout"
  );

  store.updateExperiment("foo", { active: false });

  await cleanup();
});

add_task(async function test_getRolloutForFeature_fromSyncCache() {
  const { store, cleanup } = await setupTest();
  const rollout = NimbusTestUtils.factories.rollout.withFeatureConfig("foo", {
    featureId: "aboutwelcome",
    value: { enabled: true },
  });

  store.addEnrollment(rollout);
  // New uninitialized store will return data from sync cache
  // before init
  const newStore = NimbusTestUtils.stubs.store();

  Assert.ok(
    Services.prefs.getStringPref(`${SYNC_DEFAULTS_PREF_BRANCH}aboutwelcome`),
    "Sync cache is set"
  );
  Assert.equal(
    newStore.getRolloutForFeature(rollout.featureIds[0]).slug,
    rollout.slug,
    "Should return back the same rollout"
  );
  Assert.deepEqual(
    newStore.getRolloutForFeature(rollout.featureIds[0]).branch.features[0],
    rollout.branch.features[0],
    "Should return back the same feature"
  );

  store.updateExperiment("foo", { active: false });

  await cleanup();
});

add_task(async function test_remoteRollout() {
  const { store, initExperimentAPI, cleanup } = await setupTest({
    init: false,
  });
  const featureUpdateStub = sinon.stub();

  const rollout = NimbusTestUtils.factories.rollout.withFeatureConfig("foo", {
    featureId: "aboutwelcome",
    value: { enabled: true },
  });

  store.on("featureUpdate:aboutwelcome", featureUpdateStub);

  await initExperimentAPI();

  store.addEnrollment(rollout);

  Assert.ok(
    Services.prefs.getStringPref(`${SYNC_DEFAULTS_PREF_BRANCH}aboutwelcome`),
    "Sync cache is set"
  );

  store.updateExperiment(rollout.slug, { active: false });

  Assert.ok(featureUpdateStub.calledTwice, "Called for add and remove");
  Assert.ok(
    store.get(rollout.slug),
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

  const rollout = NimbusTestUtils.factories.rollout.withFeatureConfig("foo", {
    featureId: "aboutwelcome",
    value: { remote: true },
  });
  store.addEnrollment(rollout);

  Assert.ok(
    Services.prefs.getStringPref(`${SYNC_DEFAULTS_PREF_BRANCH}aboutwelcome`),
    "Stored in pref"
  );

  store.updateExperiment("foo", { active: false });

  await cleanup();
});

add_task(async function test_syncDataStore_getDefault() {
  const { store, cleanup } = await setupTest();

  const rollout = NimbusTestUtils.factories.rollout.withFeatureConfig(
    "aboutwelcome-slug",
    { featureId: "aboutwelcome", value: { remote: true } }
  );

  await store.addEnrollment(rollout);

  Assert.ok(
    Services.prefs.getStringPref(`${SYNC_DEFAULTS_PREF_BRANCH}aboutwelcome`)
  );

  const restoredRollout = store.getRolloutForFeature("aboutwelcome");

  Assert.ok(restoredRollout);
  Assert.ok(
    restoredRollout.branch.features[0].value.remote,
    "Restore data from pref"
  );

  store.updateExperiment(rollout.slug, { active: false });

  await cleanup();
});

add_task(async function test_addEnrollment_rollout() {
  const { sandbox, store, initExperimentAPI, cleanup } = await setupTest({
    init: false,
  });

  const stub = sandbox.stub();
  const value = { bar: true };
  const rollout = NimbusTestUtils.factories.rollout.withFeatureConfig("foo", {
    featureId: "aboutwelcome",
    value,
  });

  store._onFeatureUpdate("aboutwelcome", stub);

  await initExperimentAPI();

  store.addEnrollment(rollout);

  Assert.deepEqual(
    store.getRolloutForFeature("aboutwelcome"),
    rollout,
    "should return the stored value"
  );
  Assert.equal(stub.callCount, 1, "Called once on update");
  Assert.equal(
    stub.firstCall.args[1],
    "rollout-updated",
    "Called for correct reason"
  );

  store.updateExperiment("foo", { active: false });

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

  const experiment = NimbusTestUtils.factories.experiment.withFeatureConfig(
    "foo",
    {
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
    }
  );

  store.addEnrollment(experiment);
  const branch = Services.prefs.getBranch(`${SYNC_DATA_PREF_BRANCH}purple.`);

  const newStore = NimbusTestUtils.stubs.store();
  Assert.deepEqual(
    newStore.getExperimentForFeature("purple").branch.features[0].value,
    experiment.branch.features[0].value,
    "Returns the same value"
  );

  // Cleanup
  store.updateExperiment(experiment.slug, { active: false });
  Assert.ok(
    !Services.prefs.getStringPref(`${SYNC_DATA_PREF_BRANCH}purple`, ""),
    "Experiment cleanup"
  );
  Assert.deepEqual(branch.getChildList(""), [], "Variables are also removed");

  await cleanup();
  cleanupFeature();
});

add_task(async function test_cleanupOldRecipes() {
  const { sandbox, manager, store, cleanup } = await setupTest();

  // We are intentionally putting some invalid data into the ExperimentStore.
  // NimbusTestUtils replaces addEnrollment with a version that does schema
  // validation, which we explicitly want to avoid for `inactiveNoLastSeen`
  // below.
  store.addEnrollment.restore();

  const stub = sandbox.stub(store, "_removeEntriesByKeys");

  const NOW = Date.now();
  const SIX_HOURS = 6 * 3600 * 1000;
  const ONE_DAY = 4 * SIX_HOURS;
  const ONE_YEAR = 365.25 * 24 * 3600 * 1000;
  const ONE_MONTH = Math.floor(ONE_YEAR / 12);

  const active = NimbusTestUtils.factories.experiment("active-6hrs", {
    active: true,
    lastSeen: new Date(NOW - SIX_HOURS).toJSON(),
  });

  const inactiveToday = NimbusTestUtils.factories.experiment(
    "inactive-recent",
    {
      active: false,
      unenrollReason: "unknown",
      lastSeen: new Date(NOW - SIX_HOURS).toJSON(),
    }
  );

  const inactiveSixMonths = NimbusTestUtils.factories.experiment(
    "inactive-6mo",
    {
      active: false,
      unenrollReason: "unknown",
      lastSeen: new Date(NOW - 6 * ONE_MONTH),
    }
  );

  const inactiveUnderTwelveMonths = NimbusTestUtils.factories.experiment(
    "inactive-under-12mo",
    {
      active: false,
      unenrollReason: "unknown",
      lastSeen: new Date(NOW - ONE_YEAR + ONE_DAY).toJSON(),
    }
  );

  const inactiveOverTwelveMonths = NimbusTestUtils.factories.experiment(
    "inactive-over-12mo",
    {
      active: false,
      unenrollReason: "unknown",
      lastSeen: new Date(NOW - ONE_YEAR - ONE_DAY).toJSON(),
    }
  );

  const inactiveNoLastSeen = NimbusTestUtils.factories.experiment(
    "inactive-unknown",
    {
      active: false,
      unenrollReason: "unknown",
    }
  );
  delete inactiveNoLastSeen.lastSeen;

  store.addEnrollment(active);
  await store._addEnrollmentToDatabase(
    active,
    NimbusTestUtils.factories.recipe.withFeatureConfig(
      active.slug,
      active.branch.features[0]
    )
  );
  store.addEnrollment(inactiveToday);
  await store._addEnrollmentToDatabase(
    inactiveToday,
    NimbusTestUtils.factories.recipe.withFeatureConfig(
      inactiveToday.slug,
      inactiveToday.branch.features[0]
    )
  );
  store.addEnrollment(inactiveSixMonths);
  await store._addEnrollmentToDatabase(
    inactiveSixMonths,
    NimbusTestUtils.factories.recipe.withFeatureConfig(
      inactiveSixMonths.slug,
      inactiveSixMonths.branch.features[0]
    )
  );
  store.addEnrollment(inactiveUnderTwelveMonths);
  await store._addEnrollmentToDatabase(
    inactiveUnderTwelveMonths,
    NimbusTestUtils.factories.recipe.withFeatureConfig(
      inactiveUnderTwelveMonths.slug,
      inactiveUnderTwelveMonths.branch.features[0]
    )
  );
  store.addEnrollment(inactiveOverTwelveMonths);
  await store._addEnrollmentToDatabase(
    inactiveOverTwelveMonths,
    NimbusTestUtils.factories.recipe.withFeatureConfig(
      inactiveOverTwelveMonths.slug,
      inactiveOverTwelveMonths.branch.features[0]
    )
  );

  // There is a NOT NULL constraint that prevents adding this enrollment to the
  // database.
  store.addEnrollment(inactiveNoLastSeen);

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
      branchSlug: inactiveOverTwelveMonths.branch.slug,
      active: false,
      unenrollReason: inactiveOverTwelveMonths.unenrollReason,
      lastSeen: inactiveOverTwelveMonths.lastSeen,
      source: inactiveOverTwelveMonths.source,
    }
  );

  store._cleanupOldRecipes();

  Assert.ok(stub.calledOnce, "Recipe cleanup called");
  Assert.equal(
    stub.firstCall.args[0].length,
    2,
    "We call to remove enrollments"
  );
  Assert.equal(
    stub.firstCall.args[0][0],
    inactiveOverTwelveMonths.slug,
    "Should remove expired enrollment"
  );
  Assert.equal(
    stub.firstCall.args[0][1],
    inactiveNoLastSeen.slug,
    "Should remove invalid enrollment"
  );

  await NimbusTestUtils.waitForActiveEnrollments([active.slug]);

  // There should still be a row for enrollments in the other profile.
  await NimbusTestUtils.waitForActiveEnrollments([], {
    profileId: otherProfileId,
  });
  await NimbusTestUtils.waitForInactiveEnrollment(
    inactiveOverTwelveMonths.slug,
    { profileId: otherProfileId }
  );

  await manager.unenroll(active.slug);

  await cleanup();
});

add_task(async function test_restore() {
  let storePath;
  {
    const store = NimbusTestUtils.stubs.store();
    await store.init();

    store.addEnrollment(NimbusTestUtils.factories.experiment("experiment"));
    store.addEnrollment(
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
    const experimentEnrollment =
      NimbusTestUtils.factories.experiment.withFeatureConfig("experiment", {
        featureId: "no-feature-firefox-desktop",
      });

    const rolloutRecipe = NimbusTestUtils.factories.recipe.withFeatureConfig(
      "rollout",
      { featureId: "no-feature-firefox-desktop" },
      { isRollout: true }
    );
    const rolloutEnrollment =
      NimbusTestUtils.factories.experiment.withFeatureConfig(
        "rollout",
        { featureId: "no-feature-firefox-desktop" },
        { isRollout: true }
      );

    const inactiveExperimentEnrollment =
      NimbusTestUtils.factories.experiment.withFeatureConfig(
        "inactive",
        { featureId: "no-feature-firefox-desktop" },
        { active: false }
      );

    store.addEnrollment(experimentEnrollment);
    await store._addEnrollmentToDatabase(
      experimentEnrollment,
      experimentRecipe
    );

    store.addEnrollment(rolloutEnrollment);
    await store._addEnrollmentToDatabase(rolloutEnrollment, rolloutRecipe);

    store.addEnrollment(inactiveExperimentEnrollment);
    await store._addEnrollmentToDatabase(inactiveExperimentEnrollment, null);

    // We should expect to see three successful databaseWrite events.
    const events = Glean.nimbusEvents.databaseWrite
      .testGetValue("events")
      .map(ev => ev.extra);

    Assert.deepEqual(events, [
      { success: "true" },
      { success: "true" },
      { success: "true" },
    ]);

    storePath = await NimbusTestUtils.saveStore(store);
  }

  // Initializing the store above will submit the event we care about. Disregard
  // any metrics previously recorded.
  Services.fog.testResetFOG();

  const { cleanup } = await NimbusTestUtils.setupTest({
    storePath,
    clearTelemetry: true,
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
    },
  ]);

  await NimbusTestUtils.cleanupManager(["rollout", "experiment"]);
  await cleanup();
});
