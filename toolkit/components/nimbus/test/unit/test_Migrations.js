/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const {
  LABS_MIGRATION_FEATURE_MAP,
  LEGACY_NIMBUS_MIGRATION_PREF,
  MigrationError,
  NIMBUS_MIGRATION_PREFS,
  NimbusMigrations,
} = ChromeUtils.importESModule("resource://nimbus/lib/Migrations.sys.mjs");

const { NimbusTelemetry } = ChromeUtils.importESModule(
  "resource://nimbus/lib/Telemetry.sys.mjs"
);

const { ProfilesDatastoreService } = ChromeUtils.importESModule(
  "moz-src:///toolkit/profile/ProfilesDatastoreService.sys.mjs"
);

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

add_setup(async function setup() {
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

  const { initExperimentAPI, ...ctx } = await NimbusTestUtils.setupTest({
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
    // If the test only specifies some of the phases ensure that there are
    // placeholders so that NimbusMigrations doesn't get mad.
    const migrationsStub = Object.assign(
      Object.fromEntries(
        Object.values(NimbusMigrations.Phase).map(phase => [phase, []])
      ),
      migrations
    );

    sandbox.stub(NimbusMigrations, "MIGRATIONS").get(() => migrationsStub);
  }

  if (init) {
    await initExperimentAPI();
  } else {
    ctx.initExperimentAPI = initExperimentAPI;
  }

  return ctx;
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

const IMPORT_TO_SQL_MIGRATION = NimbusMigrations.MIGRATIONS[
  NimbusMigrations.Phase.AFTER_STORE_INITIALIZED
].find(m => m.name === "import-enrollments-to-sql");

add_task(async function testMigrateEnrollmentsToSql() {
  const PREFFLIPS_EXPERIMENT_VALUE = {
    prefs: {
      "foo.bar.baz": {
        branch: "default",
        value: "prefFlips-experiment-value",
      },
    },
  };
  let storePath;

  const experiments = [
    NimbusTestUtils.factories.recipe.withFeatureConfig(
      "experiment-1",
      {
        branchSlug: "experiment-1",
        featureId: "no-feature-firefox-desktop",
      },
      {
        bogus: "foobar",
      }
    ),
    NimbusTestUtils.factories.recipe.withFeatureConfig("experiment-2", {
      branchSlug: "experiment-2",
      featureId: "no-feature-firefox-desktop",
    }),
    NimbusTestUtils.factories.recipe.withFeatureConfig(
      "rollout-1",
      {
        featureId: "no-feature-firefox-desktop",
      },
      {
        isRollout: true,
        isFirefoxLabsOptIn: true,
        firefoxLabsTitle: "title",
        firefoxLabsDescription: "description",
        firefoxLabsDescriptionLinks: {
          foo: "https://example.com",
        },
        firefoxLabsGroup: "group",
        requiresRestart: true,
      }
    ),
    NimbusTestUtils.factories.recipe.withFeatureConfig(
      "rollout-2",
      {
        featureId: "no-feature-firefox-desktop",
      },
      { isRollout: true }
    ),
    NimbusTestUtils.factories.recipe.withFeatureConfig("setPref-experiment", {
      featureId: "nimbus-qa-1",
      value: {
        value: "qa-1",
      },
    }),
    NimbusTestUtils.factories.recipe.withFeatureConfig(
      "setPref-rollout",
      {
        featureId: "nimbus-qa-2",
        value: {
          value: "qa-2",
        },
      },
      { isRollout: true }
    ),
  ];
  const secureExperiments = [
    NimbusTestUtils.factories.recipe.withFeatureConfig("prefFlips-experiment", {
      featureId: "prefFlips",
      value: PREFFLIPS_EXPERIMENT_VALUE,
    }),
    NimbusTestUtils.factories.recipe.withFeatureConfig(
      "prefFlips-rollout",
      { featureId: "prefFlips", value: { prefs: {} } },
      { isRollout: true }
    ),
  ];

  {
    const store = NimbusTestUtils.stubs.store();

    await store.init();

    store.addEnrollment(
      NimbusTestUtils.factories.experiment.withFeatureConfig(
        "inactive-1",
        { featureId: "no-feature-firefox-desktop" },
        {
          active: false,
          unenrollReason: "reason-1",
          source: NimbusTelemetry.EnrollmentSource.RS_LOADER,
        }
      )
    );
    store.addEnrollment(
      NimbusTestUtils.factories.experiment.withFeatureConfig(
        "inactive-2",
        { branchSlug: "treatment-a", featureId: "no-feature-firefox-desktop" },
        {
          active: false,
          unenrollReason: "reason-2",
          source: NimbusTelemetry.EnrollmentSource.RS_LOADER,
        }
      )
    );
    store.addEnrollment(
      NimbusTestUtils.factories.experiment.withFeatureConfig(
        "expired-but-active",
        { featureId: "no-feature-firefox-desktop" },
        { source: NimbusTelemetry.EnrollmentSource.RS_LOADER }
      )
    );

    store.addEnrollment(
      NimbusTestUtils.factories.experiment.withFeatureConfig(
        "experiment-1",
        {
          branchSlug: "experiment-1",
          featureId: "no-feature-firefox-desktop",
        },
        { source: NimbusTelemetry.EnrollmentSource.RS_LOADER }
      )
    );
    store.addEnrollment(
      NimbusTestUtils.factories.experiment.withFeatureConfig(
        "rollout-1",
        { featureId: "no-feature-firefox-desktop" },
        {
          isRollout: true,
          source: NimbusTelemetry.EnrollmentSource.RS_LOADER,
          isFirefoxLabsOptIn: true,
          firefoxLabsTitle: "title",
          firefoxLabsDescription: "description",
          firefoxLabsDescriptionLinks: {
            foo: "https://example.com",
          },
          firefoxLabsGroup: "group",
          requiresRestart: true,
        }
      )
    );
    store.addEnrollment(
      NimbusTestUtils.factories.experiment.withFeatureConfig(
        "prefFlips-experiment",
        {
          featureId: "prefFlips",
          value: PREFFLIPS_EXPERIMENT_VALUE,
        },
        {
          source: NimbusTelemetry.EnrollmentSource.RS_LOADER,
          prefFlips: {
            originalValues: {
              "foo.bar.baz": "original-value",
            },
          },
        }
      )
    );
    store.addEnrollment(
      NimbusTestUtils.factories.experiment.withFeatureConfig(
        "setPref-experiment",
        {
          featureId: "nimbus-qa-1",
          value: { value: "qa-1" },
        },
        {
          source: NimbusTelemetry.EnrollmentSource.RS_LOADER,
          prefs: [
            {
              name: "nimbus.qa.pref-1",
              branch: "default",
              featureId: "nimbus-qa-1",
              variable: "value",
              originalValue: "original-value",
            },
          ],
        }
      )
    );
    store.addEnrollment(
      NimbusTestUtils.factories.experiment.withFeatureConfig(
        "devtools",
        {
          branchSlug: "devtools",
          featureId: "no-feature-firefox-desktop",
        },
        { source: "nimbus-devtools" }
      )
    );
    store.addEnrollment(
      NimbusTestUtils.factories.experiment.withFeatureConfig(
        "optin",
        {
          branchSlug: "force-enroll",
          featureId: "no-feature-firefox-desktop",
        },
        {
          source: NimbusTelemetry.EnrollmentSource.FORCE_ENROLLMENT,
          localizations: {
            "en-US": {
              foo: "foo",
            },
          },
        }
      )
    );

    storePath = await NimbusTestUtils.saveStore(store);
  }

  let importMigrationError = null;

  // We need to run our test *directly after* the migration completes, before
  // the rest of Nimbus has had a chance to initialize, so we know that any
  // changes to the database were from the migration and not, e.g.,
  // updateRecipes().
  async function testImportMigration() {
    try {
      const conn = await ProfilesDatastoreService.getConnection();

      const result = await conn.execute(`
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
    `);

      const dbEnrollments = Object.fromEntries(
        result.map(row => {
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

          return [processed.slug, processed];
        })
      );

      Assert.deepEqual(
        Object.keys(dbEnrollments).sort(),
        [
          "inactive-1",
          "inactive-2",
          "expired-but-active",
          "experiment-1",
          "rollout-1",
          "setPref-experiment",
          "prefFlips-experiment",
          "devtools",
          "optin",
        ].sort(),
        "Should have rows for the expected enrollments"
      );

      // The profileId is the same for every enrollment.
      const profileId = ExperimentAPI.profileId;

      function assertEnrollment(expected) {
        const enrollment = dbEnrollments[expected.slug];
        const { slug } = expected;

        function msg(s) {
          return `${slug}: ${s}`;
        }

        Assert.equal(enrollment.slug, slug, "slug matches");
        Assert.equal(enrollment.profileId, profileId, msg("profileId"));
        Assert.equal(enrollment.active, expected.active, msg("active"));
        Assert.equal(
          enrollment.unenrollReason,
          expected.unenrollReason,
          msg("unenrollReason")
        );
        Assert.deepEqual(
          enrollment.setPrefs,
          expected.setPrefs,
          msg("setPrefs")
        );
        Assert.deepEqual(
          enrollment.prefFlips,
          expected.prefFlips,
          msg("prefFlips")
        );
        Assert.equal(enrollment.source, expected.source, msg("source"));

        Assert.ok(
          typeof enrollment.lastSeen === "string",
          msg("lastSeen serialized as string")
        );

        Assert.ok(
          typeof enrollment.recipe === "object" && enrollment.recipe !== null,
          msg("recipe is object")
        );

        const requiredRecipeFields = [
          "slug",
          "userFacingName",
          "userFacingDescription",
          "featureIds",
          "isRollout",
          "localizations",
          "isFirefoxLabsOptIn",
          "firefoxLabsDescription",
          "firefoxLabsDescriptionLinks",
          "firefoxLabsGroup",
          "requiresRestart",
          "branches",
        ];

        for (const recipeField of requiredRecipeFields) {
          Assert.ok(
            Object.hasOwn(enrollment.recipe, recipeField),
            msg(`recipe has ${recipeField} field`)
          );
        }

        const storeEnrollment = ExperimentAPI.manager.store.get(slug);

        Assert.equal(enrollment.recipe.slug, slug, msg("recipe.slug"));
        Assert.equal(
          enrollment.recipe.isRollout,
          storeEnrollment.isRollout,
          msg("recipe.isRollout")
        );
        Assert.ok(
          enrollment.recipe.branches.find(
            b => b.slug === enrollment.branchSlug
          ),
          msg("recipe has branch matching branchSlug")
        );

        for (const [i, branch] of enrollment.recipe.branches.entries()) {
          Assert.ok(
            typeof branch.ratio === "number",
            msg(`recipe.branches[${i}].ratio is a number`)
          );

          Assert.ok(
            typeof branch.features === "object" && branch.features !== null,
            msg(`recipe.branches[${i}].features is an object`)
          );

          for (const featureId of enrollment.recipe.featureIds) {
            const idx = branch.features.findIndex(
              fc => fc.featureId === featureId
            );
            Assert.ok(
              idx !== -1,
              msg(
                `recipe.branches[${i}].features[${idx}].featureId = ${featureId}`
              )
            );

            const featureConfig = branch.features[idx];
            Assert.ok(
              typeof featureConfig.value === "object" &&
                featureConfig.value !== null,
              msg(`recipe.branches[${i}].features[${idx}].value is an object`)
            );
          }
        }
      }

      assertEnrollment({
        slug: "inactive-1",
        branchSlug: "control",
        active: false,
        unenrollReason: "reason-1",
        setPrefs: null,
        prefFlips: null,
        source: NimbusTelemetry.EnrollmentSource.RS_LOADER,
      });

      assertEnrollment({
        slug: "inactive-2",
        branchSlug: "treatment-a",
        active: false,
        unenrollReason: "reason-2",
        setPrefs: null,
        prefFlips: null,
        source: NimbusTelemetry.EnrollmentSource.RS_LOADER,
      });

      assertEnrollment({
        slug: "expired-but-active",
        branchSlug: "control",
        active: true,
        unenrollReason: null,
        setPrefs: null,
        prefFlips: null,
        source: NimbusTelemetry.EnrollmentSource.RS_LOADER,
      });

      assertEnrollment({
        slug: "experiment-1",
        branchSlug: "experiment-1",
        active: true,
        unenrollReason: null,
        setPrefs: null,
        prefFlips: null,
        source: NimbusTelemetry.EnrollmentSource.RS_LOADER,
      });

      Assert.equal(
        dbEnrollments["experiment-1"].recipe.bogus,
        "foobar",
        `experiment-1: entire recipe from Remote Settings is captured`
      );

      assertEnrollment({
        slug: "rollout-1",
        branchSlug: "control",
        active: true,
        unenrollReason: null,
        setPrefs: null,
        prefFlips: null,
        source: NimbusTelemetry.EnrollmentSource.RS_LOADER,
      });

      Assert.ok(
        dbEnrollments["rollout-1"].recipe.isFirefoxLabsOptIn,
        `rollout-1: recipe.isFirefoxLabsOptIn`
      );
      Assert.equal(
        dbEnrollments["rollout-1"].recipe.firefoxLabsTitle,
        "title",
        `rollout-1: recipe.firefoxLabsTitle`
      );
      Assert.equal(
        dbEnrollments["rollout-1"].recipe.firefoxLabsDescription,
        "description",
        `rollout-1: recipe.firefoxLabsDescription`
      );
      Assert.deepEqual(
        dbEnrollments["rollout-1"].recipe.firefoxLabsDescriptionLinks,
        {
          foo: "https://example.com",
        },
        `rollout-1: recipe.firefoxLabsDescriptionLinks`
      );
      Assert.equal(
        dbEnrollments["rollout-1"].recipe.firefoxLabsGroup,
        "group",
        `rollout-1: recipe.firefoxLabsGroup`
      );
      Assert.ok(
        dbEnrollments["rollout-1"].recipe.requiresRestart,
        `rollout-1: recipe.requiresRestart`
      );

      assertEnrollment({
        slug: "prefFlips-experiment",
        branchSlug: "control",
        active: true,
        unenrollReason: null,
        setPrefs: null,
        prefFlips: {
          originalValues: {
            "foo.bar.baz": "original-value",
          },
        },
        source: NimbusTelemetry.EnrollmentSource.RS_LOADER,
      });

      assertEnrollment({
        slug: "setPref-experiment",
        branchSlug: "control",
        active: true,
        unenrollReason: null,
        setPrefs: [
          {
            name: "nimbus.qa.pref-1",
            branch: "default",
            featureId: "nimbus-qa-1",
            variable: "value",
            originalValue: "original-value",
          },
        ],
        prefFlips: null,
        source: NimbusTelemetry.EnrollmentSource.RS_LOADER,
      });

      assertEnrollment({
        slug: "devtools",
        branchSlug: "devtools",
        active: true,
        unenrollReason: null,
        setPrefs: null,
        prefFlips: null,
        source: "nimbus-devtools",
      });

      assertEnrollment({
        slug: "optin",
        branchSlug: "force-enroll",
        active: true,
        unenrollReason: null,
        setPrefs: null,
        prefFlips: null,
        source: NimbusTelemetry.EnrollmentSource.FORCE_ENROLLMENT,
      });

      Assert.deepEqual(
        dbEnrollments.optin.recipe.localizations,
        {
          "en-US": {
            foo: "foo",
          },
        },
        "optin: localizations is captured in recipe"
      );
    } catch (e) {
      importMigrationError = e;
    }
  }

  const { cleanup } = await setupTest({
    storePath,
    experiments,
    secureExperiments,
    migrations: {
      [NimbusMigrations.Phase.AFTER_STORE_INITIALIZED]: [
        IMPORT_TO_SQL_MIGRATION,
        { name: "test-import-migration", fn: testImportMigration },
      ],
    },
  });

  // NimbusMigrations swallows errors
  if (importMigrationError) {
    throw importMigrationError;
  }

  await NimbusTestUtils.cleanupManager([
    "experiment-1",
    "experiment-2",
    "rollout-1",
    "rollout-2",
    "prefFlips-experiment",
    "prefFlips-rollout",
    "setPref-experiment",
    "setPref-rollout",
    "devtools",
    "optin",
  ]);

  await cleanup();

  // On unenroll, we should 'reset' foo.bar.baz, nimbus.qa.pref-1, and nimbus.qa.pref-2.
  Assert.equal(
    Services.prefs.getStringPref("foo.bar.baz"),
    "original-value",
    "foo.bar.baz restored"
  );
  Assert.equal(
    Services.prefs.getStringPref("nimbus.qa.pref-1"),
    "original-value",
    "nimbus.qs.pref-1 restored"
  );
  Assert.ok(
    !Services.prefs.prefHasUserValue("nimbus.qa.pref-2"),
    "nimbus.qa.pref-2 restored"
  );
  Services.prefs.deleteBranch("foo.bar.baz");
  Services.prefs.deleteBranch("nimbus.qa.pref-1");
  Services.prefs.deleteBranch("nimbus.qa.pref-2");
});
