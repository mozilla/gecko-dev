/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ExperimentAPI: "resource://nimbus/ExperimentAPI.sys.mjs",
  FirefoxLabs: "resource://nimbus/FirefoxLabs.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "log", () => {
  const { Logger } = ChromeUtils.importESModule(
    "resource://messaging-system/lib/Logger.sys.mjs"
  );
  return new Logger("NimbusMigrations");
});

function migration(name, fn) {
  return { name, fn };
}

export const NIMBUS_MIGRATION_PREF = "nimbus.migrations.latest";

export const LABS_MIGRATION_FEATURE_MAP = {
  "auto-pip": "firefox-labs-auto-pip",
  "urlbar-ime-search": "firefox-labs-urlbar-ime-search",
  "jpeg-xl": "firefox-labs-jpeg-xl",
};

async function migrateFirefoxLabsEnrollments() {
  await lazy.ExperimentAPI._rsLoader.finishedUpdating();
  await lazy.ExperimentAPI._rsLoader.withUpdateLock(
    async () => {
      const labs = await lazy.FirefoxLabs.create();

      let didEnroll = false;

      for (const [feature, slug] of Object.entries(
        LABS_MIGRATION_FEATURE_MAP
      )) {
        const pref =
          lazy.NimbusFeatures[feature].manifest.variables.enabled.setPref.pref;

        if (!labs.get(slug)) {
          // If the recipe is not available then either it is no longer live or
          // the targeting did not match.
          continue;
        }

        if (!Services.prefs.getBoolPref(pref, false)) {
          // Only enroll if the migration pref is set.
          continue;
        }

        await labs.enroll(slug, "control");

        // We need to overwrite the original pref value stored in the
        // ExperimentStore so that unenrolling will disable the feature.
        const enrollment = lazy.ExperimentAPI._manager.store.get(slug);
        if (!enrollment) {
          lazy.log.error(`Enrollment with ${slug} should exist but does not`);
          continue;
        }
        if (!enrollment.active) {
          lazy.log.error(
            `Enrollment with slug ${slug} should be active but is not.`
          );
          continue;
        }

        const prefEntry = enrollment.prefs?.find(entry => entry.name === pref);
        if (!prefEntry) {
          lazy.log.error(
            `Enrollment with slug ${slug} does not set pref ${pref}`
          );
          continue;
        }

        didEnroll = true;
        prefEntry.originalValue = false;
      }

      if (didEnroll) {
        // Trigger a save of the ExperimentStore since we've changed some data
        // structures without using set().
        // We do not have to sync these changes to child processes because the
        // data is only used in the parent process.
        lazy.ExperimentAPI._manager.store._store.saveSoon();
      }
    },
    { mode: "shared" }
  );
}

export class MigrationError extends Error {
  static Reason = Object.freeze({
    UNKNOWN: "unknown",
  });

  constructor(reason) {
    super(`Migration error: ${reason}`);
    this.reason = reason;
  }
}

export const NimbusMigrations = {
  migration,

  /**
   * Apply any outstanding migrations.
   */
  async applyMigrations() {
    const latestMigration = Services.prefs.getIntPref(
      NIMBUS_MIGRATION_PREF,
      -1
    );
    let lastSuccess = latestMigration;

    lazy.log.debug(`applyMigrations: latestMigration = ${latestMigration}`);

    for (let i = latestMigration + 1; i < this.MIGRATIONS.length; i++) {
      const migration = this.MIGRATIONS[i];

      lazy.log.debug(
        `applyMigrations: applying migration ${i}: ${migration.name}`
      );

      try {
        await migration.fn();
      } catch (e) {
        lazy.log.error(
          `applyMigrations: error running migration ${i} (${migration.name}): ${e}`
        );

        let reason = MigrationError.Reason.UNKNOWN;
        if (e instanceof MigrationError) {
          reason = e.reason;
        }

        Glean.nimbusEvents.migration.record({
          migration_id: migration.name,
          success: false,
          error_reason: reason,
        });

        break;
      }

      lastSuccess = i;

      lazy.log.debug(
        `applyMigrations: applied migration ${i}: ${migration.name}`
      );

      Glean.nimbusEvents.migration.record({
        migration_id: migration.name,
        success: true,
      });
    }

    if (latestMigration != lastSuccess) {
      Services.prefs.setIntPref(NIMBUS_MIGRATION_PREF, lastSuccess);
    }
  },

  MIGRATIONS: [
    migration("firefox-labs-enrollments", migrateFirefoxLabsEnrollments),
  ],
};
