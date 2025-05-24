/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const {
  LABS_MIGRATION_FEATURE_MAP,
  LEGACY_NIMBUS_MIGRATION_PREF,
  MigrationError,
  NIMBUS_MIGRATION_PREFS,
  NimbusMigrations,
} = ChromeUtils.importESModule("resource://nimbus/lib/Migrations.sys.mjs");

/** @typedef {import("../../lib/Migrations.sys.mjs").Migration} Migration */
/** @typedef {import("../../lib/Migrations.sys.mjs").Phase} Phase */

function mockLabsRecipes(targeting = "true") {
  return Object.entries(LABS_MIGRATION_FEATURE_MAP).map(([featureId, slug]) =>
    NimbusTestUtils.factories.recipe.withFeatureConfig(
      slug,
      { featureId, value: { enabled: true } },
      {
        isRollout: true,
        isFirefoxLabsOptIn: true,
        firefoxLabsTitle: `${featureId}-placeholder-title`,
        firefoxLabsDescription: `${featureId}-placeholder-desc`,
        firefoxLabsDescriptionLinks: null,
        firefoxLabsGroup: "placeholder",
        targeting,
      }
    )
  );
}

function getEnabledPrefForFeature(featureId) {
  return NimbusFeatures[featureId].manifest.variables.enabled.setPref.pref;
}

add_setup(function setup() {
  Services.fog.initializeFOG();
});

/**
 * Setup a test environment.
 *
 * @param {object} options
 * @param {number?} options.legacyMigrationState
 *        The value of the legacy migration pref.
 * @param {Record<Phase, number>?} options.migrationState
 *        The value that should be set for the Nimbus migration prefs. If
 *        not provided, the pref will be unset.
 * @param {Record<Phase, Migration[]>} options.migrations
 *        An array of migrations that will replace the regular set of migrations
 *        for the duration of the test.
 * @param {object[]} options.recipes
 *        An array of experiment recipes that will be returned by the
 *        RemoteSettingsExperimentLoader for the duration of the test.
 * @param {object} options.args
 *       Options to pass to to NimbusTestutils.setupNimbusTest.
 */

async function setupTest({
  legacyMigrationState,
  migrationState,
  migrations,
  init = true,
  ...args
} = {}) {
  Assert.ok(
    !Services.prefs.prefHasUserValue(LEGACY_NIMBUS_MIGRATION_PREF),
    `legacy migration pref should be unset`
  );

  for (const [phase, pref] of Object.keys(NIMBUS_MIGRATION_PREFS)) {
    Assert.ok(
      !Services.prefs.prefHasUserValue(pref),
      `${phase} migration pref should be unset`
    );
  }

  const {
    initExperimentAPI,
    cleanup: baseCleanup,
    ...ctx
  } = await NimbusTestUtils.setupTest({
    init: false,
    clearTelemetry: true,
    ...args,
  });

  const { sandbox } = ctx;

  if (migrationState) {
    for (const [phase, value] of Object.entries(migrationState)) {
      Services.prefs.setIntPref(NIMBUS_MIGRATION_PREFS[phase], value);
    }
  }

  if (typeof legacyMigrationState !== "undefined") {
    Services.prefs.setIntPref(
      LEGACY_NIMBUS_MIGRATION_PREF,
      legacyMigrationState
    );
  }

  if (migrations) {
    sandbox.stub(NimbusMigrations, "MIGRATIONS").get(() => migrations);
  }

  if (init) {
    await initExperimentAPI();
  } else {
    ctx.initExperimentAPI = initExperimentAPI;
  }

  return {
    ...ctx,
    async cleanup() {
      await baseCleanup();
      Services.prefs.deleteBranch("nimbus.migrations");
    },
  };
}

function makeMigrations(phase, count) {
  const migrations = [];
  for (let i = 0; i < count; i++) {
    migrations.push(
      NimbusMigrations.migration(`test-migration-${phase}-${i}`, sinon.stub())
    );
  }
  return migrations;
}

add_task(async function test_migration_unset() {
  info("Testing NimbusMigrations with no migration pref set");
  const startupMigrations = makeMigrations(
    NimbusMigrations.Phase.INIT_STARTED,
    2
  );
  const storeMigrations = makeMigrations(
    NimbusMigrations.Phase.AFTER_STORE_INITIALIZED,
    2
  );
  const updateMigrations = makeMigrations(
    NimbusMigrations.Phase.AFTER_REMOTE_SETTINGS_UPDATE,
    1
  );

  const { cleanup } = await setupTest({
    migrations: {
      [NimbusMigrations.Phase.INIT_STARTED]: startupMigrations,
      [NimbusMigrations.Phase.AFTER_STORE_INITIALIZED]: storeMigrations,
      [NimbusMigrations.Phase.AFTER_REMOTE_SETTINGS_UPDATE]: updateMigrations,
    },
  });

  Assert.ok(
    startupMigrations[0].fn.calledOnce,
    `${startupMigrations[0].name} should be called once`
  );
  Assert.ok(
    startupMigrations[1].fn.calledOnce,
    `${startupMigrations[1].name} should be called once`
  );

  Assert.equal(
    Services.prefs.getIntPref(
      NIMBUS_MIGRATION_PREFS[NimbusMigrations.Phase.INIT_STARTED]
    ),
    1,
    "Migration pref should be updated"
  );

  Assert.ok(
    storeMigrations[0].fn.calledOnce,
    `${storeMigrations[0].name} should be called once`
  );
  Assert.ok(
    storeMigrations[1].fn.calledOnce,
    `${storeMigrations[1].name} should be called once`
  );

  Assert.equal(
    Services.prefs.getIntPref(
      NIMBUS_MIGRATION_PREFS[NimbusMigrations.Phase.AFTER_STORE_INITIALIZED]
    ),
    1,
    "Migration pref should be updated"
  );

  Assert.ok(
    updateMigrations[0].fn.calledOnce,
    `${updateMigrations[0].name} should be called once`
  );

  Assert.equal(
    Services.prefs.getIntPref(
      NIMBUS_MIGRATION_PREFS[
        NimbusMigrations.Phase.AFTER_REMOTE_SETTINGS_UPDATE
      ]
    ),
    0,
    "Migration pref should be updated"
  );

  Assert.deepEqual(
    Glean.nimbusEvents.migration.testGetValue().map(event => event.extra),
    [
      {
        success: "true",
        migration_id: startupMigrations[0].name,
      },
      {
        success: "true",
        migration_id: startupMigrations[1].name,
      },
      {
        success: "true",
        migration_id: storeMigrations[0].name,
      },
      {
        success: "true",
        migration_id: storeMigrations[1].name,
      },
      {
        success: "true",
        migration_id: updateMigrations[0].name,
      },
    ]
  );

  await cleanup();
});

add_task(async function test_migration_partially_done() {
  info("Testing NimbusMigrations with some migrations completed");
  const startupMigrations = makeMigrations(
    NimbusMigrations.Phase.INIT_STARTED,
    2
  );
  const storeMigrations = makeMigrations(
    NimbusMigrations.Phase.AFTER_STORE_INITIALIZED,
    2
  );
  const updateMigrations = makeMigrations(
    NimbusMigrations.Phase.AFTER_REMOTE_SETTINGS_UPDATE,
    2
  );

  const { cleanup } = await setupTest({
    migrationState: {
      [NimbusMigrations.Phase.INIT_STARTED]: 0,
      [NimbusMigrations.Phase.AFTER_STORE_INITIALIZED]: 0,
      [NimbusMigrations.Phase.AFTER_REMOTE_SETTINGS_UPDATE]: 0,
    },
    migrations: {
      [NimbusMigrations.Phase.INIT_STARTED]: startupMigrations,
      [NimbusMigrations.Phase.AFTER_STORE_INITIALIZED]: storeMigrations,
      [NimbusMigrations.Phase.AFTER_REMOTE_SETTINGS_UPDATE]: updateMigrations,
    },
  });

  Assert.ok(
    startupMigrations[0].fn.notCalled,
    `${startupMigrations[0].name} should not be called`
  );
  Assert.ok(
    startupMigrations[1].fn.calledOnce,
    `${startupMigrations[1].name} should be called once`
  );

  Assert.ok(
    storeMigrations[0].fn.notCalled,
    `${updateMigrations[0].name} should not be called`
  );
  Assert.ok(
    storeMigrations[1].fn.calledOnce,
    `${updateMigrations[1].name} should be called once`
  );

  Assert.ok(
    updateMigrations[0].fn.notCalled,
    `${updateMigrations[0].name} should not be called`
  );
  Assert.ok(
    updateMigrations[1].fn.calledOnce,
    `${updateMigrations[1].name} should be called once`
  );

  Assert.deepEqual(
    Glean.nimbusEvents.migration.testGetValue().map(event => event.extra),
    [
      {
        success: "true",
        migration_id: startupMigrations[1].name,
      },
      {
        success: "true",
        migration_id: storeMigrations[1].name,
      },
      {
        success: "true",
        migration_id: updateMigrations[1].name,
      },
    ]
  );

  await cleanup();
});

add_task(async function test_migration_throws() {
  info(
    "Testing NimbusMigrations with a migration that throws an unknown error"
  );
  const startupMigrations = makeMigrations(
    NimbusMigrations.Phase.INIT_STARTED,
    3
  );
  startupMigrations[1].fn.throws(
    new Error(`${startupMigrations[1].name} failed`)
  );

  const storeMigrations = makeMigrations(
    NimbusMigrations.Phase.INIT_STARTED,
    3
  );
  storeMigrations[1].fn.throws(new Error(`${storeMigrations[1].name} failed`));

  const updateMigrations = makeMigrations(
    NimbusMigrations.Phase.AFTER_REMOTE_SETTINGS_UPDATE,
    3
  );
  updateMigrations[1].fn.throws(
    new Error(`${updateMigrations[1].name} failed`)
  );

  const { cleanup } = await setupTest({
    migrations: {
      [NimbusMigrations.Phase.INIT_STARTED]: startupMigrations,
      [NimbusMigrations.Phase.AFTER_STORE_INITIALIZED]: storeMigrations,
      [NimbusMigrations.Phase.AFTER_REMOTE_SETTINGS_UPDATE]: updateMigrations,
    },
  });

  Assert.ok(
    startupMigrations[0].fn.calledOnce,
    `${startupMigrations[0].name} should be called once`
  );
  Assert.ok(
    startupMigrations[1].fn.calledOnce,
    `${startupMigrations[1].name} should be called once`
  );
  Assert.ok(
    startupMigrations[2].fn.notCalled,
    `${startupMigrations[2].name} should not be called`
  );

  Assert.ok(
    storeMigrations[0].fn.calledOnce,
    `${storeMigrations[0].name} should be called once`
  );
  Assert.ok(
    storeMigrations[1].fn.calledOnce,
    `${storeMigrations[1].name} should be called once`
  );
  Assert.ok(
    storeMigrations[2].fn.notCalled,
    `${storeMigrations[2].name} should not be called`
  );

  Assert.ok(
    updateMigrations[0].fn.calledOnce,
    `${updateMigrations[0].name} should be called once`
  );
  Assert.ok(
    updateMigrations[1].fn.calledOnce,
    `${updateMigrations[1].name} should be called once`
  );
  Assert.ok(
    updateMigrations[2].fn.notCalled,
    `${updateMigrations[2].name} should not be called`
  );

  Assert.equal(
    Services.prefs.getIntPref(
      NIMBUS_MIGRATION_PREFS[NimbusMigrations.Phase.INIT_STARTED]
    ),
    0,
    "Migration pref should only be set to 0"
  );
  Assert.equal(
    Services.prefs.getIntPref(
      NIMBUS_MIGRATION_PREFS[NimbusMigrations.Phase.AFTER_STORE_INITIALIZED]
    ),
    0,
    "Migration pref should only be set to 0"
  );
  Assert.equal(
    Services.prefs.getIntPref(
      NIMBUS_MIGRATION_PREFS[
        NimbusMigrations.Phase.AFTER_REMOTE_SETTINGS_UPDATE
      ]
    ),
    0,
    "Migration pref should only be set to 0"
  );

  Assert.deepEqual(
    Glean.nimbusEvents.migration.testGetValue().map(event => event.extra),
    [
      {
        success: "true",
        migration_id: startupMigrations[0].name,
      },
      {
        success: "false",
        migration_id: startupMigrations[1].name,
        error_reason: MigrationError.Reason.UNKNOWN,
      },
      {
        success: "true",
        migration_id: storeMigrations[0].name,
      },
      {
        success: "false",
        migration_id: storeMigrations[1].name,
        error_reason: MigrationError.Reason.UNKNOWN,
      },
      {
        success: "true",
        migration_id: updateMigrations[0].name,
      },
      {
        success: "false",
        migration_id: updateMigrations[1].name,
        error_reason: MigrationError.Reason.UNKNOWN,
      },
    ]
  );

  await cleanup();
});

add_task(async function test_migration_throws_MigrationError() {
  info(
    "Testing NimbusMigrations with a migration that throws a MigrationError"
  );
  const startupMigrations = makeMigrations(
    NimbusMigrations.Phase.INIT_STARTED,
    3
  );
  startupMigrations[1].fn.throws(new MigrationError("bogus"));

  const storeMigrations = makeMigrations(
    NimbusMigrations.Phase.AFTER_STORE_INITIALIZED,
    3
  );
  storeMigrations[1].fn.throws(new MigrationError("bogus"));

  const updateMigrations = makeMigrations(
    NimbusMigrations.Phase.AFTER_REMOTE_SETTINGS_UPDATE,
    3
  );
  updateMigrations[1].fn.throws(new MigrationError("bogus"));

  const { cleanup } = await setupTest({
    migrations: {
      [NimbusMigrations.Phase.INIT_STARTED]: startupMigrations,
      [NimbusMigrations.Phase.AFTER_STORE_INITIALIZED]: storeMigrations,
      [NimbusMigrations.Phase.AFTER_REMOTE_SETTINGS_UPDATE]: updateMigrations,
    },
  });

  Assert.ok(
    startupMigrations[0].fn.calledOnce,
    `${startupMigrations[0].name} should be called once`
  );
  Assert.ok(
    startupMigrations[1].fn.calledOnce,
    `${startupMigrations[1].name} should be called once`
  );
  Assert.ok(
    startupMigrations[2].fn.notCalled,
    `${startupMigrations[2].name} should not be called`
  );

  Assert.ok(
    storeMigrations[0].fn.calledOnce,
    `${storeMigrations[0].name} should be called once`
  );
  Assert.ok(
    storeMigrations[1].fn.calledOnce,
    `${storeMigrations[1].name} should be called once`
  );
  Assert.ok(
    storeMigrations[2].fn.notCalled,
    `${storeMigrations[2].name} should not be called`
  );

  Assert.ok(
    updateMigrations[0].fn.calledOnce,
    `${updateMigrations[0].name} should be called once`
  );
  Assert.ok(
    updateMigrations[1].fn.calledOnce,
    `${updateMigrations[1].name} should be called once`
  );
  Assert.ok(
    updateMigrations[2].fn.notCalled,
    `${updateMigrations[2].name} should not be called`
  );

  Assert.equal(
    Services.prefs.getIntPref(
      NIMBUS_MIGRATION_PREFS[NimbusMigrations.Phase.INIT_STARTED]
    ),
    0,
    "Migration pref should only be set to 0"
  );
  Assert.equal(
    Services.prefs.getIntPref(
      NIMBUS_MIGRATION_PREFS[NimbusMigrations.Phase.AFTER_STORE_INITIALIZED]
    ),
    0,
    "Migration pref should only be set to 0"
  );
  Assert.equal(
    Services.prefs.getIntPref(
      NIMBUS_MIGRATION_PREFS[
        NimbusMigrations.Phase.AFTER_REMOTE_SETTINGS_UPDATE
      ]
    ),
    0,
    "Migration pref should only be set to 0"
  );

  Assert.deepEqual(
    Glean.nimbusEvents.migration.testGetValue().map(event => event.extra),
    [
      {
        success: "true",
        migration_id: startupMigrations[0].name,
      },
      {
        success: "false",
        migration_id: startupMigrations[1].name,
        error_reason: "bogus",
      },
      {
        success: "true",
        migration_id: storeMigrations[0].name,
      },
      {
        success: "false",
        migration_id: storeMigrations[1].name,
        error_reason: "bogus",
      },
      {
        success: "true",
        migration_id: updateMigrations[0].name,
      },
      {
        success: "false",
        migration_id: updateMigrations[1].name,
        error_reason: "bogus",
      },
    ]
  );

  await cleanup();
});

const LEGACY_TO_MULTIPHASE_MIGRATION =
  NimbusMigrations.MIGRATIONS[NimbusMigrations.Phase.INIT_STARTED][0];

add_task(async function test_migration_legacyToMultiphase_unset() {
  const migrations = makeMigrations(NimbusMigrations.Phase.INIT_STARTED, 2);

  const { cleanup } = await setupTest({
    migrations: {
      [NimbusMigrations.Phase.INIT_STARTED]: [LEGACY_TO_MULTIPHASE_MIGRATION],
      [NimbusMigrations.Phase.AFTER_REMOTE_SETTINGS_UPDATE]: migrations,
    },
  });

  Assert.ok(
    migrations[0].fn.calledOnce,
    `${migrations[0].name} should be called`
  );
  Assert.ok(
    migrations[1].fn.calledOnce,
    `${migrations[1].name} should be called`
  );

  Assert.equal(
    Services.prefs.getIntPref(
      NIMBUS_MIGRATION_PREFS[NimbusMigrations.Phase.INIT_STARTED]
    ),
    0,
    "before-manager-startup phase pref should be set"
  );

  Assert.equal(
    Services.prefs.getIntPref(
      NIMBUS_MIGRATION_PREFS[
        NimbusMigrations.Phase.AFTER_REMOTE_SETTINGS_UPDATE
      ]
    ),
    1,
    "after-remote-setttings-update phase pref should be set"
  );

  Assert.ok(
    !Services.prefs.prefHasUserValue(LEGACY_NIMBUS_MIGRATION_PREF),
    "legacy phase pref is unset"
  );

  await cleanup();
});

add_task(async function test_migration_legacyToMultiphase_partial() {
  const migrations = makeMigrations(
    NimbusMigrations.Phase.AFTER_REMOTE_SETTINGS_UPDATE,
    3
  );

  const { cleanup } = await setupTest({
    legacyMigrationState: 1,
    migrations: {
      [NimbusMigrations.Phase.INIT_STARTED]: [LEGACY_TO_MULTIPHASE_MIGRATION],
      [NimbusMigrations.Phase.AFTER_REMOTE_SETTINGS_UPDATE]: migrations,
    },
  });

  Assert.ok(
    migrations[0].fn.notCalled,
    `${migrations[0].name} should not be called`
  );
  Assert.ok(
    migrations[1].fn.notCalled,
    `${migrations[1].name} should not be called`
  );
  Assert.ok(
    migrations[2].fn.calledOnce,
    `${migrations[2].name} should be called`
  );

  Assert.equal(
    Services.prefs.getIntPref(
      NIMBUS_MIGRATION_PREFS[NimbusMigrations.Phase.INIT_STARTED]
    ),
    0,
    "before-manager-startup phase pref should be set"
  );

  Assert.equal(
    Services.prefs.getIntPref(
      NIMBUS_MIGRATION_PREFS[
        NimbusMigrations.Phase.AFTER_REMOTE_SETTINGS_UPDATE
      ]
    ),
    2,
    "after-remote-setttings-update phase pref should be set"
  );

  Assert.ok(
    !Services.prefs.prefHasUserValue(LEGACY_NIMBUS_MIGRATION_PREF),
    "legacy phase pref is unset"
  );

  await cleanup();
});

add_task(async function test_migration_legacyToMultiphase_complete() {
  const migrations = makeMigrations(
    NimbusMigrations.Phase.AFTER_REMOTE_SETTINGS_UPDATE,
    2
  );
  const { cleanup } = await setupTest({
    legacyMigrationState: 1,
    migrations: {
      [NimbusMigrations.Phase.INIT_STARTED]: [LEGACY_TO_MULTIPHASE_MIGRATION],
      [NimbusMigrations.Phase.AFTER_REMOTE_SETTINGS_UPDATE]: migrations,
    },
  });

  Assert.ok(migrations[0].fn.notCalled, `${migrations[0].name} not called`);
  Assert.ok(migrations[1].fn.notCalled, `${migrations[1].name} not called`);

  Assert.equal(
    Services.prefs.getIntPref(
      NIMBUS_MIGRATION_PREFS[NimbusMigrations.Phase.INIT_STARTED]
    ),
    0,
    "before-manager-startup phase pref should be set"
  );
  Assert.equal(
    Services.prefs.getIntPref(
      NIMBUS_MIGRATION_PREFS[
        NimbusMigrations.Phase.AFTER_REMOTE_SETTINGS_UPDATE
      ]
    ),
    1,
    "after-remote-setttings-update phase pref should be set"
  );
  Assert.ok(
    !Services.prefs.prefHasUserValue(LEGACY_NIMBUS_MIGRATION_PREF),
    "legacy phase pref is unset"
  );

  await cleanup();
});

const FIREFOX_LABS_MIGRATION =
  NimbusMigrations.MIGRATIONS[
    NimbusMigrations.Phase.AFTER_REMOTE_SETTINGS_UPDATE
  ][0];

add_task(async function test_migration_firefoxLabsEnrollments() {
  async function doTest(features) {
    info(
      `Testing NimbusMigrations migrates Firefox Labs features ${JSON.stringify(features)}`
    );
    const prefs = features.map(getEnabledPrefForFeature);
    for (const pref of prefs) {
      Services.prefs.setBoolPref(pref, true);
    }
    const { manager, cleanup } = await setupTest({
      experiments: mockLabsRecipes("true"),
      migrations: {
        [NimbusMigrations.Phase.AFTER_REMOTE_SETTINGS_UPDATE]: [
          FIREFOX_LABS_MIGRATION,
        ],
      },
    });

    Assert.deepEqual(
      await manager
        .getAllOptInRecipes()
        .then(recipes => recipes.map(recipe => recipe.slug).toSorted()),
      Object.values(LABS_MIGRATION_FEATURE_MAP).toSorted(),
      "The labs recipes should be available"
    );

    for (const [feature, slug] of Object.entries(LABS_MIGRATION_FEATURE_MAP)) {
      const enrollmentExpected = features.includes(feature);
      const enrollment = manager.store.get(slug);

      if (enrollmentExpected) {
        Assert.ok(
          !!enrollment,
          `There should be an enrollment for slug ${slug}`
        );

        const pref = getEnabledPrefForFeature(feature);
        Assert.equal(
          Services.prefs.getBoolPref(pref),
          true,
          `Pref ${pref} should be set after enrollment`
        );

        await manager.unenroll(slug);
        Assert.equal(
          Services.prefs.getBoolPref(pref),
          false,
          `Pref ${pref} should be unset after unenrollment`
        );
        Assert.ok(
          !Services.prefs.prefHasUserValue(pref),
          `Pref ${pref} should not be set on the user branch`
        );
      } else {
        Assert.ok(
          !enrollment,
          `There should not be an enrollment for slug ${slug}`
        );
      }
    }

    Assert.deepEqual(
      Glean.nimbusEvents.migration.testGetValue().map(event => event.extra),
      [
        {
          success: "true",
          migration_id: "firefox-labs-enrollments",
        },
      ]
    );

    await cleanup();
  }

  await doTest([]);

  for (const feature of Object.keys(LABS_MIGRATION_FEATURE_MAP)) {
    await doTest([feature]);
  }

  await doTest(Object.keys(LABS_MIGRATION_FEATURE_MAP));
});

add_task(async function test_migration_firefoxLabsEnrollments_falseTargeting() {
  // Some of the features will be limited to specific channels.
  // We don't need to test that targeting evaluation itself works, so we'll just
  // test with hardcoded targeting.
  info(
    `Testing NimbusMigration does not migrate Firefox Labs features when targeting is false`
  );
  const prefs = Object.keys(LABS_MIGRATION_FEATURE_MAP).map(
    getEnabledPrefForFeature
  );
  for (const pref of prefs) {
    Services.prefs.setBoolPref(pref, true);
  }
  const { manager, cleanup } = await setupTest({
    experiments: mockLabsRecipes("false"),
    migrations: {
      [NimbusMigrations.Phase.AFTER_REMOTE_SETTINGS_UPDATE]: [
        FIREFOX_LABS_MIGRATION,
      ],
    },
  });

  Assert.deepEqual(
    await manager.getAllOptInRecipes(),
    [],
    "There should be no opt-in recipes"
  );

  for (const pref of prefs) {
    Assert.ok(
      Services.prefs.getBoolPref(pref),
      `Pref ${pref} should be unchanged`
    );

    Services.prefs.clearUserPref(pref);
  }

  for (const slug of Object.values(LABS_MIGRATION_FEATURE_MAP)) {
    Assert.ok(
      typeof manager.store.get(slug) === "undefined",
      `There should be no store entry for ${slug}`
    );
  }

  Assert.deepEqual(
    Glean.nimbusEvents.migration.testGetValue().map(event => event.extra),
    [
      {
        success: "true",
        migration_id: "firefox-labs-enrollments",
      },
    ]
  );

  await cleanup();
});

add_task(async function test_migration_firefoxLabsEnrollments_idempotent() {
  info("Testing the firefox-labs-enrollments migration is idempotent");

  const prefs = Object.keys(LABS_MIGRATION_FEATURE_MAP).map(
    getEnabledPrefForFeature
  );

  for (const pref of prefs) {
    Services.prefs.setBoolPref(pref, true);
  }

  const recipes = mockLabsRecipes("true");

  // Get the store into a partially migrated state (i.e., we have enrolled in at least one
  // experiment but the migration pref has not updated).
  {
    const manager = NimbusTestUtils.stubs.manager();
    await manager.store.init();
    await manager.onStartup();

    manager.enroll(recipes[0], "rs-loader", { branchSlug: "control" });

    await NimbusTestUtils.saveStore(manager.store);

    removePrefObservers(manager);
    assertNoObservers(manager);
  }

  const { manager, cleanup } = await setupTest({
    experiments: recipes,
    migrations: {
      [NimbusMigrations.Phase.AFTER_REMOTE_SETTINGS_UPDATE]: [
        FIREFOX_LABS_MIGRATION,
      ],
    },
  });

  Assert.equal(
    Services.prefs.getIntPref(
      NIMBUS_MIGRATION_PREFS[
        NimbusMigrations.Phase.AFTER_REMOTE_SETTINGS_UPDATE
      ]
    ),
    0,
    "Migration pref updated"
  );
  Assert.deepEqual(
    Glean.nimbusEvents.migration.testGetValue().map(ev => ev.extra),
    [
      {
        migration_id: "firefox-labs-enrollments",
        success: "true",
      },
    ]
  );

  for (const { slug } of recipes) {
    await manager.unenroll(slug);
  }

  await cleanup();

  for (const pref of prefs) {
    Services.prefs.clearUserPref(pref);
  }
});
