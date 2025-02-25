const { NormandyMigrations } = ChromeUtils.importESModule(
  "resource://normandy/NormandyMigrations.sys.mjs"
);

decorate_task(
  withMockPreferences(),
  async function testApplyMigrations({ mockPreferences }) {
    const migrationsAppliedPref = "app.normandy.migrationsApplied";
    mockPreferences.set(migrationsAppliedPref, 0);

    await NormandyMigrations.applyAll();

    is(
      Services.prefs.getIntPref(migrationsAppliedPref),
      NormandyMigrations.migrations.length,
      "All migrations should have been applied"
    );
  }
);

decorate_task(
  withMockPreferences(),
  async function testPrefMigration({ mockPreferences }) {
    const legacyPref = "extensions.shield-recipe-client.test";
    const migratedPref = "app.normandy.test";
    mockPreferences.set(legacyPref, 1);

    ok(
      Services.prefs.prefHasUserValue(legacyPref),
      "Legacy pref should have a user value before running migration"
    );
    ok(
      !Services.prefs.prefHasUserValue(migratedPref),
      "Migrated pref should not have a user value before running migration"
    );

    await NormandyMigrations.applyOne(0);

    ok(
      !Services.prefs.prefHasUserValue(legacyPref),
      "Legacy pref should not have a user value after running migration"
    );
    ok(
      Services.prefs.prefHasUserValue(migratedPref),
      "Migrated pref should have a user value after running migration"
    );
    is(
      Services.prefs.getIntPref(migratedPref),
      1,
      "Value should have been migrated"
    );

    Services.prefs.clearUserPref(migratedPref);
  }
);
