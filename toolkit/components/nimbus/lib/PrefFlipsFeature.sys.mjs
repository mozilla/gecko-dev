/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ExperimentManager: "resource://nimbus/lib/ExperimentManager.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  PrefUtils: "resource://normandy/lib/PrefUtils.sys.mjs",
  UnenrollmentCause: "resource://nimbus/lib/ExperimentManager.sys.mjs",
});

const FEATURE_ID = "prefFlips";

/**
 * The value of a pref.
 *
 * A value of null indicates that the preference will be cleared.
 *
 * @typedef {string | number | boolean | null} PrefValue
 */

/**
 * Prefs can be set on two branches: the user branch or the default branch.
 *
 * The user branch will be persisted to a file on disk and be restored early
 * during startup, whereas the default branch is set to default values every
 * startup.
 *
 * @typedef {"user" | "default"} PrefBranch
 */

/**
 * Information about an individual pref tracked by this feaure.
 *
 * @typedef {object} PrefEntry
 *
 * @property {PrefBranch} branch
 *           The branch the pref was set on.
 *
 * @property {PrefValue} originalValue
 *           The original value of the pref on that branch. This may be `null`
 *           to indicate there was no value.
 *
 * @property {PrefValue} value
 *           The value of the pref. This may be `null` to indicate the pref was
 *           cleared.
 *
 * @property {nsIPrefObserver} observer
 *           The observer that is called when the pref changes to cause
 *           unenrollment.
 *
 * @property {Set<string>} slugs
 *           The slugs of all active enrollments that set this pref.
 */

/**
 * Configuration for an individual preference in the feature configuration.
 *
 * @typedef {object} PrefConfig
 *
 * @property {PrefBranch} branch
 *           The requested branch to set the pref on.
 *
 * @property {PrefValue} value
 *           The requested value to set the pref to.
 */

/**
 * The prefFlips feature.
 *
 * This should *only* be instantiated by the active `ExperimentManager` in the
 * parent process.
 */
export class PrefFlipsFeature {
  /**
   * Whether or not the feature has been initialized.
   *
   * @type {boolean}
   */
  #initialized;

  /**
   * Whether or not the feature is currently in a feature update callback. This
   * flag is checked to ensure we don't recursively trigger update callbacks by
   * causing unenrollments.
   *
   * @type {boolean}
   */
  #updating;

  /**
   * All the prefs that the feature is tracking.
   *
   * @type {Map<string, PrefEntry>}
   */
  #prefs;

  /**
   * A mapping of prefs to the slugs of experiments that set that pref.
   *
   * It is guaranteed that all these slugs set the pref to the same value on the
   * same branch.
   *
   * @type {Map<string, Set<string>>}
   */
  #prefsBySlug;

  static get FEATURE_ID() {
    return FEATURE_ID;
  }

  /**
   * Construct a new prefFlips feature.
   *
   * @param {object} options
   * @param {ExperimentManager} options.manager
   *        The ExperimentManager that owns this feature.
   */
  constructor({ manager }) {
    this.manager = manager;

    this.#initialized = false;
    this.#updating = false;
    this.#prefs = new Map();
    this.#prefsBySlug = new Map();
  }

  /**
   * Intialize the prefFlips feature.
   *
   * This will re-hydrate `this.#prefs` from the active enrollment (if any) and
   * register any necessary pref observers.
   *
   * `onFeatureUpdate` will be called for any future feature changes.
   */
  init() {
    if (this.#initialized) {
      return;
    }

    this.#initialized = true;

    const enrollments = lazy.NimbusFeatures[FEATURE_ID].getAllEnrollments();

    this.#updating = true;
    for (const {
      meta: { slug },
      value: { prefs = {} },
    } of enrollments) {
      const enrollment = this.manager.store.get(slug);
      this.#restoreEnrollment(enrollment, prefs);
    }
    this.#updating = false;

    lazy.NimbusFeatures[FEATURE_ID].onUpdate(this.#onFeatureUpdate.bind(this));
  }

  /**
   * Return the orginal value of the pref on the specific branch if it is set by
   * this feature.
   *
   * @params {string} pref
   *         The pref to get the original value of.
   *
   * @params {PrefBranch} branch
   *         The requested branch for the pref.
   *
   * @returns {PrefValue | undefined}
   *          The original value of the pref on the specified branch. If the
   *          pref is not set, `undefined` will be returned. If the pref is
   *          being cleared by an experiment, `null` will be returned.
   */
  _getOriginalValue(pref, branch) {
    const entry = this.#prefs.get(pref);
    if (!entry || entry.branch !== branch) {
      return undefined;
    }

    return entry.originalValue;
  }

  /**
   * Return the number of registered prefs.
   *
   * This is only exposed so that tests can assert on its value.
   *
   * @returns {number}
   *          The number of registered prefs.
   */
  get _registeredPrefCount() {
    return this.#prefs.size;
  }

  /**
   * Handle a potential conflict between a pref flip we own and a regular
   * setPref.
   *
   * This should only be called by the global `ExperimentManager` when it is
   * enrolling.
   *
   * @param {string} conflictingSlug
   *        The slug the ExperimentManager is enrolling.
   *
   * @param {string[]} prefs
   *        The prefs that the experiment will set.
   */
  _handleSetPrefConflict(conflictingSlug, prefs) {
    // Suppress feature updates while we unenroll from these enrollments.
    this.#updating = true;

    for (const pref of prefs) {
      const entry = this.#prefs.get(pref);
      if (entry) {
        for (const slug of entry.slugs) {
          this.manager.unenroll(
            slug,
            lazy.UnenrollmentCause.PrefFlipsConflict(conflictingSlug)
          );
          this.#removeEnrollment(slug);
        }
      }
    }

    this.#updating = false;
  }

  /**
   * Triggered when a feature update happens on the prefFlips feature.
   *
   * Only one update can happen at a time.
   */
  #onFeatureUpdate() {
    if (this.#updating) {
      return;
    }

    this.#updating = true;
    this.#onFeatureUpdateImpl();
    this.#updating = false;
  }

  /**
   * Handle a feature update.
   *
   * N.B.: This can only be called when `#updating` is true to prevent recursive
   *       updates from triggering.
   */
  #onFeatureUpdateImpl() {
    const enrollments = lazy.NimbusFeatures[FEATURE_ID].getAllEnrollments();

    const activeSlugs = new Set(enrollments.map(e => e.meta.slug));
    const knownSlugs = new Set(this.#prefsBySlug.keys());
    const inactiveSlugs = knownSlugs.difference(activeSlugs);
    const newSlugs = activeSlugs.difference(knownSlugs);

    for (const slug of inactiveSlugs) {
      this.#removeEnrollment(slug);

      // Remove the cached original values on the inactive enrollment.
      const enrollment = this.manager.store.get(slug);
      delete enrollment.prefFlips;
    }

    for (const slug of newSlugs) {
      const enrollment = this.manager.store.get(slug);
      this.#addEnrollment(enrollment);
    }

    if (inactiveSlugs.size || newSlugs.size) {
      // If we've modified any enrollments in the store we must ensure that
      // there is a save queued.
      this.manager.store._store.saveSoon();
    }
  }

  async _annotateEnrollment(enrollment) {
    const { featureIds } = enrollment;
    if (!featureIds.includes(FEATURE_ID)) {
      return;
    }

    const prefs =
      lazy.ExperimentManager.getFeatureConfigFromBranch(
        enrollment.branch,
        FEATURE_ID
      ).value.prefs ?? {};

    const originalValues = this.manager._handlePrefFlipsConflict(
      enrollment.slug,
      Object.entries(prefs).map(([pref, { branch }]) => [pref, branch])
    );

    for (const [pref, { branch }] of Object.entries(prefs)) {
      if (this.#prefs.has(pref)) {
        originalValues[pref] = this.#prefs.get(pref).originalValue;
      } else {
        originalValues[pref] = Object.hasOwn(originalValues, pref)
          ? originalValues[pref]
          : lazy.PrefUtils.getPref(pref, { branch });
      }
    }

    // Cache the original values one the enrollment so they can be restored upon
    // unenrollment.
    if (!Object.hasOwn(enrollment, "prefFlips")) {
      enrollment.prefFlips = {};
    }

    enrollment.prefFlips.originalValues = originalValues;
  }

  /**
   * Start tracking an enrollment.
   *
   * This will register prefs for the enrollment. If we have already registered
   * any of the prefs for this enrollment, the values and branches must match or
   * this enrollment will be unenrolled.
   *
   * NB: The enrollment must have already been annotated by a call to
   * {@link _annotateEnrollment}, which occurrs in `ExperimentManager.enroll()`.
   *
   * @param {object} enrollment
   *        The enrollment we are tracking.
   */
  #addEnrollment(enrollment) {
    const { slug } = enrollment;
    const prefs =
      lazy.ExperimentManager.getFeatureConfigFromBranch(
        enrollment.branch,
        FEATURE_ID
      ).value.prefs ?? {};

    const originalValues = enrollment.prefFlips.originalValues;

    const prefsBySlug = new Set();
    this.#prefsBySlug.set(slug, prefsBySlug);

    for (const [pref, { branch, value }] of Object.entries(prefs)) {
      try {
        if (this.#prefs.has(pref)) {
          this.#registerExistingPref(slug, pref, branch, value);
        } else {
          this.#registerNewPref(
            slug,
            pref,
            branch,
            value,
            originalValues[pref]
          );
        }

        prefsBySlug.add(pref);
      } catch (e) {
        this.#unenrollForFailure(enrollment, pref);
        return;
      }
    }
  }

  /**
   * Start tracking an enrollment at startup.
   *
   * If we fail to set a pref, this will trigger an unenrollment for the
   * feature.
   *
   * N.B.: This can only be called when `#updating` is true to prevent recursive
   *       updates from triggering.
   *
   * @param {object} enrollment
   *        The enrollment we are restoring.
   *
   * @param {Record<string, PrefConfig>}
   *        The prefs that this enrollment will set.
   */
  #restoreEnrollment(enrollment, prefs) {
    const { slug } = enrollment;

    const prefsBySlug = new Set();
    this.#prefsBySlug.set(slug, prefsBySlug);

    for (const [pref, { branch, value }] of Object.entries(prefs)) {
      try {
        if (this.#prefs.has(pref)) {
          this.#registerExistingPref(slug, pref, branch, value);
        } else {
          const originalValue = enrollment.prefFlips.originalValues[pref];
          this.#registerNewPref(slug, pref, branch, value, originalValue);
        }

        prefsBySlug.add(pref);
      } catch (e) {
        console.error(
          `Failed to restore enrollment ${enrollment.slug} because of ${pref}:`,
          e
        );
        this.#unenrollForFailure(enrollment, pref);
        return;
      }
    }
  }

  /**
   * Stop tracking the enrollment for the given slug.
   *
   * This will stop unregister all the prefs for this slug. If no enrollments
   * are tracking a pref, it will be reset to its original value.
   *
   * @param {string} slug
   *        The slug for the enrollment we are no longer tracking.
   */
  #removeEnrollment(slug) {
    const prefs = this.#prefsBySlug.get(slug);
    if (!prefs) {
      return;
    }

    this.#prefsBySlug.delete(slug);

    for (const pref of prefs) {
      const entry = this.#prefs.get(pref);

      entry.slugs.delete(slug);

      if (entry.slugs.size == 0) {
        Services.prefs.removeObserver(pref, entry.observer);
        this.#prefs.delete(pref);

        try {
          lazy.PrefUtils.setPref(pref, entry.originalValue, {
            branch: entry.branch,
          });
        } catch (e) {
          console.error(`Failed to restore pref ${pref}:`, e);
        }
      }
    }
  }

  /**
   * Register a new pref for the enrollment with the given slug.
   *
   * @param {string} slug
   *        The slug of the enrollment.
   *
   * @param {string} pref
   *        The pref that we are setting.
   *
   * @param {PrefBranch} branch
   *        The branch the pref will be set on.
   *
   * @param {PrefValue} value
   *        The value we will set the pref to.
   *
   * @param {PrefValue} originalValue
   *        The original value of the pref.
   *
   *        This is always required because we cannot determine the original
   *        value correctly if we have previously unenrolled a setPref
   *        experiment for this pref on the default branch.
   *
   * @throws {PrefFlipsFailedError}
   *         If we fail to set the pref. This should cause the caller to
   *         unenroll from the enrollment.
   */
  #registerNewPref(slug, pref, branch, value, originalValue) {
    /** @type nsIPrefObserver */
    const observer = (_aSubject, _aTopic, aData) => {
      // This observer will be called for changes to `name` as well as any
      // other pref that begins with `name.`, so we have to filter to
      // exactly the pref we care about.
      if (aData === pref) {
        this.#onPrefChanged(pref);
      }
    };

    /** @type PrefEntry */
    const entry = {
      branch,
      originalValue,
      value: value ?? null,
      observer,
      slugs: new Set([slug]),
    };

    try {
      lazy.PrefUtils.setPref(pref, value ?? null, { branch });
    } catch (e) {
      throw new PrefFlipsFailedError(pref, value);
    }

    Services.prefs.addObserver(pref, entry.observer);
    this.#prefs.set(pref, entry);
  }

  /**
   * Register an existing pref for a new enrollment.
   *
   * @param {string} slug
   *        The slug for the new enrollment.
   *
   * @param {string} pref
   *        The pref that we are setting.
   *
   * @param {PrefBranch} branch
   *        The branch the pref will be set on.
   *
   * @param {PrefValue} value
   *        The value we will set the pref to.
   *
   * @throws {PrefFlipsFailedError}
   *         If either the pref branch or pref value do not match the existing
   *         registration. This should cause the caller to unenroll from the
   *         enrollment.
   */
  #registerExistingPref(slug, pref, branch, value) {
    const entry = this.#prefs.get(pref);

    if (entry.branch !== branch || entry.value !== value) {
      throw new PrefFlipsFailedError(pref, value);
    }

    entry.slugs.add(slug);
  }

  /**
   * Triggered when a pref unexpectedly changes.
   *
   * @param {string} pref
   *        The pref that changed.
   */
  #onPrefChanged(pref) {
    if (this.#updating) {
      return;
    }

    if (this.manager._prefs.get(pref)?.enrollmentChanging) {
      return;
    }

    this.#updating = true;
    this.#onPrefChangedImpl(pref);
    this.#updating = false;
  }

  /**
   * Handle an unexpected preference change.
   *
   * N.B.: This can only be called when `#updating` is true to prevent updates
   *       from triggering.
   *
   * @param {string} pref
   *        The pref that changed.
   */
  #onPrefChangedImpl(pref) {
    const entry = this.#prefs.get(pref);
    if (!entry) {
      return;
    }

    // The pref was changed by something outside our control, so before we start
    // rolling back enrollments we need to remove it from all our internal
    // state, otherwise we will end up overwriting the change that just happened.
    this.#prefs.delete(pref);
    Services.prefs.removeObserver(pref, entry.observer);

    // Compute how the pref changed so we can report it in telemetry.
    const cause = lazy.UnenrollmentCause.ChangedPref({
      name: pref,
      branch: PrefFlipsFeature.determinePrefChangeBranch(
        pref,
        entry.branch,
        entry.value
      ),
    });

    // Now we can trigger unenrollment of these slugs. Every enrollment settings
    // this pref has to stop tracking it.
    for (const slug of entry.slugs) {
      this.#prefsBySlug.get(slug).delete(pref);

      // TODO(bug 1956082): This is an async method that we are not awaiting.
      //
      // This function is only ever called inside a nsIPrefObserver callback,
      // which are invoked without `await`. Awaiting here breaks tests in
      // test_prefFlips.js, which assert about the values of prefs *after* we
      // trigger unenrollment.
      //
      // There is no good way to synchronize this behaviour yet to satisfy tests and
      // the only thing that is being deferred are the database writes, which we
      // and our caller don't care about.
      this.manager.unenroll(slug, cause);
    }

    // Because we've unenrolled first, we can trigger a regular feature update
    // to update our state.
    this.#onFeatureUpdateImpl();
  }

  /**
   * Unenroll the given enrollment due to a failure to set the given pref.
   *
   * N.B.: This must only be called when `#updating` is true to prevent
   *       recursive updates from triggering.
   *
   * @param {object} enrollment
   *        The enrollment that we are unenrolling.
   *
   * @param {string} pref
   *        The name of the pref that we failed to set.
   *
   * @returns {string}
   *          The type of pref, or "invalid" if there is no value for the pref.
   */
  #unenrollForFailure(enrollment, pref) {
    const rawType = Services.prefs.getPrefType(pref);
    let prefType = "invalid";

    switch (rawType) {
      case Ci.nsIPrefBranch.PREF_BOOL:
        prefType = "bool";
        break;

      case Ci.nsIPrefBranch.PREF_STRING:
        prefType = "string";
        break;

      case Ci.nsIPrefBranch.PREF_INT:
        prefType = "int";
        break;
    }

    this.manager._unenroll(
      enrollment,
      lazy.UnenrollmentCause.PrefFlipsFailed(pref, prefType)
    );

    // This function is only called during an update, so we have to remove the
    // enrollment ourselves instead of relying on the feature update callback.
    this.#removeEnrollment(enrollment.slug);
  }

  /**
   * Determine on what branch did a pref change happen.
   *
   * @param {string} pref
   *        The name of the pref.
   *
   * @param {string} expectedBranch
   *        The branch we were setting the pref on.
   *
   * @param {PrefValue} expectedValue
   *        The value we were setting for the pref on `expectedBranch`.
   *
   * @returns {PrefBranch}
   *          The branch the pref change occurred on.
   */
  static determinePrefChangeBranch(pref, expectedBranch, expectedValue) {
    // We want to know what branch was changed so we can know if we should
    // restore prefs (.e.,g if we have a pref set on the user branch and the
    // user branch changed, we do not want to then overwrite the user's choice).

    // This is not complicated if a pref simply changed. However, we must also
    // detect `nsIPrefBranch::clearUserPref()`, which wipes out the user branch
    // and leaves the default branch untouched. That is where this gets
    // complicated.

    if (Services.prefs.prefHasUserValue(pref)) {
      // If there is a user branch value, then the user branch changed, because
      // a change to the default branch wouldn't have triggered the observer.
      return "user";
    } else if (!Services.prefs.prefHasDefaultValue(pref)) {
      // If there is no user branch value *or* default branch avlue, then the
      // user branch must have been cleared because you cannot clear the default
      // branch.
      return "user";
    } else if (expectedBranch === "default") {
      const value = lazy.PrefUtils.getPref(pref, { branch: "default" });
      if (value === expectedValue) {
        // The pref we control was set on the default branch and still matches
        // the expected value. Therefore, the user branch must have been
        // cleared.
        return "user";
      }
      // The default value branch does not match the value we expect, so it
      // must have just changed.
      return "default";
    }
    return "user";
  }
}

/**
 * Thrown when the prefFlips feature fails to set a pref.
 *
 * @property {string} pref
 *           The pref that triggered this exception.
 */
class PrefFlipsFailedError extends Error {
  /**
   * Construct a new PrefFlipsFailedError.
   *
   * @param {string} pref
   *        The pref we failed to set.
   *
   * @param {PrefValue} value
   *        The value we failed to set the pref to.
   */
  constructor(pref, value) {
    super(`The Nimbus prefFlips feature failed to set ${pref}=${value}`);
    this.pref = pref;
  }
}
