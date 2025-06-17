/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/** @import { ExperimentStore } from "./ExperimentStore.sys.mjs" */
/** @import { DeferredTask } from "../../../modules/DeferredTask.sys.mjs" */
/** @import { OpenedConnection, Sqlite } from "../../../modules/Sqlite.sys.mjs" */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  DeferredTask: "resource://gre/modules/DeferredTask.sys.mjs",
  ExperimentAPI: "resource://nimbus/ExperimentAPI.sys.mjs",
  ProfilesDatastoreService:
    "moz-src:///toolkit/profile/ProfilesDatastoreService.sys.mjs",
  Sqlite: "resource://gre/modules/Sqlite.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "log", () => {
  const { Logger } = ChromeUtils.importESModule(
    "resource://messaging-system/lib/Logger.sys.mjs"
  );
  return new Logger("NimbusEnrollments");
});

/**
 * How long should we wait before flushing changes to the database.
 *
 * This was copied from a similar constant in JSONFile.sys.mjs.
 */
const FLUSH_DELAY_MS = 1500;

// We read this prefs *once* at startup because the ExperimentStore (and
// SharedDataMap) have different initialization logic based on the state of these prefs.
//
// To ensure that changing them doesn't result in inconsistent behavaiour, they
// will only take affect after a restart.
//
// They are only mutable so they can be updated for tests.
let DATABASE_ENABLED = Services.prefs.getBoolPref(
  "nimbus.profilesdatastoreservice.enabled",
  false
);

/**
 * Handles queueing changes to the NimbusEnrollments database table in the
 * shared profiles database.
 */
export class NimbusEnrollments {
  /**
   * The ExperimentStore.
   *
   * @type {ExperimentStore}
   */
  #store;

  /**
   * The task that will flush the changes to the database on a timer.
   *
   * @type {DeferredTask}
   */
  #flushTask;

  /**
   * Our shutdown blocker that will
   * @type {(function(): void) | null}
   */
  #shutdownBlocker;

  /**
   * Whether or not we've started shutdown and will accept new pending writes.
   *
   * @type {boolean}
   */
  #finalized;

  /**
   * Pending writes that will be flushed in `#flush()`.
   *
   * The keys are enrollment slugs and the values are optional recipes. If the
   * recipe is present, a new enrollment will be created in the database.
   * Otherwise, an existing enrollment will be updated to be inactive (or
   * deleted if it cannot be found in the `ExperimentStore`).
   *
   * @type {Map<string, object | null>}
   */
  #pending;

  constructor(store) {
    this.#store = store;

    this.#flushTask = new lazy.DeferredTask(
      this.#flush.bind(this),
      FLUSH_DELAY_MS
    );

    this.#shutdownBlocker = this.finalize.bind(this);
    lazy.Sqlite.shutdown.addBlocker(
      "NimbusEnrollments: writing to database",
      this.#shutdownBlocker
    );
    this.#finalized = false;

    this.#pending = new Map();
  }

  /**
   * The number of pending writes.
   */
  get pendingWrites() {
    return this.#pending.size;
  }

  /**
   * Queue an update to the database for the given enrollment.
   *
   * @param {string} slug The slug of the enrollment.
   * @param {object | undefined} recipe If this update is for an enrollment event,
   * the recipe that resulted in the enrollment.
   */
  updateEnrollment(slug, recipe) {
    if (this.#finalized) {
      lazy.log.debug(
        `Did not queue update for enrollment ${slug}: already finalized`
      );
      return;
    }

    lazy.log.debug(`Queued update for enrollment ${slug}`);

    // Don't overwrite a pending entry that has a recipe with one that has none
    // or we will try to do the wrong query (UPDATE instead of INSERT).
    if (!this.#pending.has(slug)) {
      this.#pending.set(slug, recipe);
      this.#flushSoon();
    }
  }

  /**
   * Immediately flush all pending updates to the NimbusEnrollments table.
   *
   * ** TEST ONLY **
   */
  async _flushNow() {
    // If the flush is already running, wait for it to finish.
    if (this.#flushTask.isRunning) {
      await this.#flushTask._runningPromise;
    }

    // If the flush task is armed, disarm it and run it immediately.
    if (this.#flushTask.isArmed) {
      this.#flushTask.disarm();
      await this.#flush();
    }
  }

  /**
   * Queue a flush to happen soon (within {@link FLUSH_DELAY_MS}).
   */
  #flushSoon() {
    if (this.#finalized) {
      return;
    }

    this.#flushTask.arm();
  }

  /**
   * Flush all pending updates to the NimbusEnrollments table.
   *
   * The updates are done as a single transaction to ensure the database stays
   * in a consistent state.
   *
   * If the transaction fails, the write will be re-attempted after
   * {@link FLUSH_DELAY_MS}, unless we have already begun shutdown, in which we
   * will attempt to flush once more immediately.
   *
   * @param {object} options
   * @param {boolean} options.retryOnFailure Whether or not to retry flushing if
   * an error occurs. Should only be false if we have failed to flush once and
   * we've started shutting down.
   */
  async #flush({ retryOnFailure = true } = {}) {
    if (!this.#pending.size) {
      lazy.log.debug(`Not flushing: no changes`);
      return;
    }

    const pending = this.#pending;
    this.#pending = new Map();

    lazy.log.debug(`Flushing ${this.#pending.size} changes to database`);

    let success = true;
    try {
      const conn = await lazy.ProfilesDatastoreService.getConnection();
      if (!conn) {
        // The database has already closed. There's nothing we can do.
        //
        // This *should not happen* since we have a shutdown blocker preventing
        // the connection from being closed, but it may happen if we're already
        // in shutdown when we try to flush for the first time, e.g., during a
        // very short-lived session.
        lazy.log.debug(
          `Not flushing changes to database: connection is closed`
        );
        return;
      }

      await conn.executeTransaction(async () => {
        for (const [slug, recipe] of pending.entries()) {
          const enrollment = this.#store.get(slug);
          if (enrollment) {
            await this.#insertOrUpdateEnrollment(conn, enrollment, recipe);
          } else {
            await this.#deleteEnrollment(conn, slug);
          }
        }
      });
    } catch (e) {
      success = false;

      if (retryOnFailure) {
        // Re-queue all the pending writes that failed.
        if (this.#pending.size) {
          // If there have been store changes since the call to flush, we need to
          // include all of those and the failed writes.
          const newPending = this.#pending;
          this.#pending = pending;

          for (const [slug, recipe] of newPending.entries()) {
            this.updateEnrollment(slug, recipe);
          }
        } else {
          // If there have been no pending changes, we can just replace the set of
          // pending writes.
          this.#pending = pending;
        }

        if (!this.#finalized) {
          lazy.log.error(
            `ExperimentStore: Failed writing enrollments to NimbusEnrollments; will retry soon`,
            e
          );

          // Ensure we try to flush again if we aren't in shutdown yet.
          this.#flushSoon();
        } else {
          lazy.log.error(
            `ExperimentStore: Failed writing enrollments to NimbusEnrollments during shutdown; retrying immediately`,
            e
          );

          // If we are in our shutdown blocker, we aren't going to get another
          // chance and there's not really anything we can do except try to
          // write again immediately.
          await this.#flush({ retryOnFailure: false });
        }
      } else {
        lazy.log.error(
          `ExperimentStore: Failed writing enrollments to NimbusEnrollments`,
          e
        );
      }
    }

    Glean.nimbusEvents.databaseWrite.record({ success });
  }

  /**
   * Insert or update an enrollment.
   * @param {OpenedConnection} conn The connection to the database.
   * @param {object} enrollment The enrollment.
   * @param {object | null} recipe The recipe for the enrollment. Only non-null
   * when the initial enrollment has not already been flushed.
   */
  async #insertOrUpdateEnrollment(conn, enrollment, recipe) {
    if (recipe) {
      // This was a new enrollment at the time. It may have since unenrolled.
      await conn.executeCached(
        `
          INSERT INTO NimbusEnrollments(
            profileId,
            slug,
            branchSlug,
            recipe,
            active,
            unenrollReason,
            lastSeen,
            setPrefs,
            prefFlips,
            source
          )
          VALUES(
            :profileId,
            :slug,
            :branchSlug,
            jsonb(:recipe),
            :active,
            :unenrollReason,
            :lastSeen,
            jsonb(:setPrefs),
            jsonb(:prefFlips),
            :source
          )
          ON CONFLICT(profileId, slug)
          DO UPDATE SET
            branchSlug = excluded.branchSlug,
            recipe = excluded.recipe,
            active = excluded.active,
            unenrollReason = excluded.unenrollReason,
            lastSeen = excluded.lastSeen,
            setPrefs = excluded.setPrefs,
            prefFlips = excluded.setPrefs,
            source = excluded.source;
        `,
        {
          profileId: lazy.ExperimentAPI.profileId,
          slug: enrollment.slug,
          branchSlug: enrollment.branch.slug,
          recipe: enrollment.active ? JSON.stringify(recipe) : null,
          active: enrollment.active,
          unenrollReason: enrollment.unenrollReason ?? null,
          lastSeen: enrollment.lastSeen,
          setPrefs:
            enrollment.active && enrollment.prefs
              ? JSON.stringify(enrollment.prefs)
              : null,
          prefFlips:
            enrollment.active && enrollment.prefFlips
              ? JSON.stringify(enrollment.prefFlips)
              : null,
          source: enrollment.source,
        }
      );

      lazy.log.debug(
        `Created ${enrollment.active ? "active" : "inactive"} enrollment ${enrollment.slug}`
      );
    } else {
      // This was an unenrollment.
      await conn.executeCached(
        `
          UPDATE NimbusEnrollments SET
            recipe = null,
            active = false,
            unenrollReason = :unenrollReason,
            setPrefs = null,
            prefFlips = null
          WHERE
            profileId = :profileId AND
            slug = :slug;
        `,
        {
          profileId: lazy.ExperimentAPI.profileId,
          slug: enrollment.slug,
          unenrollReason: enrollment.unenrollReason,
        }
      );

      lazy.log.debug(`Updated enrollment ${enrollment.slug} to be inactive`);
    }
  }

  /**
   * Remove an expired enrollment from the NimbusEnrollments table.
   *
   * @param {OpenedConnection} conn The connection to the database.
   * @param {string} slug The slug of the enrollment to delete.
   */
  async #deleteEnrollment(conn, slug) {
    await conn.execute(
      `
        DELETE FROM NimbusEnrollments
        WHERE
          profileId = :profileId AND
          slug = :slug;
      `,
      {
        profileId: lazy.ExperimentAPI.profileId,
        slug,
      }
    );

    lazy.log.debug(`Deleted expired enrollment ${slug}`);
  }

  /**
   * Finalize the flush task and ensure we flush all pending enrollment updates
   * to the NimbusEnrollments table.
   *
   * As soon as this function is called, all new enrollment updates will be
   * refused.
   *
   * This is used as a shutdown blocker that blocks the {@link Sqlite.shutdown}
   * shutdown barrier.
   */
  async finalize() {
    if (this.#finalized) {
      return;
    }

    this.#finalized = true;
    await this.#flushTask.finalize();

    lazy.Sqlite.shutdown.removeBlocker(this.#shutdownBlocker);
    this.#shutdownBlocker = null;
  }

  /**
   * Whether or not writing to the NimbusEnrollments table is enabled.
   *
   * This should only be false in xpcshell tests.
   */
  static get databaseEnabled() {
    // TODO(bug 1967779): require the ProfilesDatastoreService to be initialized
    // and remove this.
    return DATABASE_ENABLED;
  }

  /**
   * Reload the database-related prefs
   *
   * ** TEST ONLY **
   */
  static _reloadPrefsForTests() {
    DATABASE_ENABLED = Services.prefs.getBoolPref(
      "nimbus.profilesdatastoreservice.enabled",
      false
    );
  }
}
