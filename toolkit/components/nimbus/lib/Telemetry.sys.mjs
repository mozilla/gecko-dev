/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  TelemetryEnvironment: "resource://gre/modules/TelemetryEnvironment.sys.mjs",
  TelemetryEvents: "resource://normandy/lib/TelemetryEvents.sys.mjs",
});

const LEGACY_TELEMETRY_EVENT_OBJECT = "nimbus_experiment";
const EXPERIMENT_ACTIVE_PREFIX = "nimbus-";

const LegacyTelemetryEvents = Object.freeze({
  ENROLL: "enroll",
  ENROLL_FAILED: "enrollFailed",
  UNENROLL: "unenroll",
  UNENROLL_FAILED: "unenrollFailed",
  VALIDATION_FAILED: "validationFailed",
});

const EnrollmentStatus = Object.freeze({
  ENROLLED: "Enrolled",
  NOT_ENROLLED: "NotEnrolled",
  DISQUALIFIED: "Disqualified",
  WAS_ENROLLED: "WasEnrolled",
  ERROR: "Error",
});

const EnrollmentStatusReason = Object.freeze({
  QUALIFIED: "Qualified",
  OPT_IN: "OptIn",
  OPT_OUT: "OptOut",
  NOT_SELECTED: "NotSelected",
  NOT_TARGETED: "NotTargeted",
  ENROLLMENTS_PAUSED: "EnrollmentsPaused",
  FEATURE_CONFLICT: "FeatureConflict",
  ERROR: "Error",
});

const EnrollmentFailureReason = Object.freeze({
  FEATURE_CONFLICT: "feature-conflict",
  NAME_CONFLICT: "name-conflict",
});

const UnenrollmentFailureReason = Object.freeze({
  ALREADY_UNENROLLED: "already-unenrolled",
  DOES_NOT_EXIST: "does-not-exist",
});

const ValidationFailureReason = Object.freeze({
  INVALID_BRANCH: "invalid-branch",
  INVALID_FEATURE: "invalid-feature",
  INVALID_RECIPE: "invalid-recipe",
  L10N_MISSING_ENTRY: "l10n-missing-entry",
  L10N_MISSING_LOCALE: "l10n-missing-locale",
  UNSUPPORTED_FEATURES: "unsupported-feature",
});

const UnenrollReason = Object.freeze({
  BRANCH_REMOVED: "branch-removed",
  BUCKETING: "bucketing",
  CHANGED_PREF: "changed-pref",
  INDIVIDUAL_OPT_OUT: "individual-opt-out",
  PREF_FLIPS_CONFLICT: "prefFlips-conflict",
  PREF_FLIPS_FAILED: "prefFlips-failed",
  PREF_VARIABLE_CHANGED: "pref-variable-changed",
  PREF_VARIABLE_MISSING: "pref-variable-missing",
  PREF_VARIABLE_NO_LONGER: "pref-variable-no-longer",
  RECIPE_NOT_SEEN: "recipe-not-seen",
  STUDIES_OPT_OUT: "studies-opt-out",
  TARGETING_MISMATCH: "targeting-mismatch",
  UNKNOWN: "unknown",

  // Validation failure can cause unenrollment.
  ...ValidationFailureReason,
});

export const NimbusTelemetry = {
  EnrollmentFailureReason,
  EnrollmentStatus,
  EnrollmentStatusReason,
  UnenrollReason,
  UnenrollmentFailureReason,
  ValidationFailureReason,

  recordEnrollment(enrollment) {
    this.setExperimentActive(enrollment);
    lazy.TelemetryEvents.sendEvent(
      LegacyTelemetryEvents.ENROLL,
      LEGACY_TELEMETRY_EVENT_OBJECT,
      enrollment.slug,
      {
        experimentType: enrollment.experimentType,
        branch: enrollment.branch.slug,
      }
    );
    Glean.nimbusEvents.enrollment.record({
      experiment: enrollment.slug,
      branch: enrollment.branch.slug,
      experiment_type: enrollment.experimentType,
    });
  },

  recordEnrollmentFailure(slug, reason) {
    lazy.TelemetryEvents.sendEvent(
      LegacyTelemetryEvents.ENROLL_FAILED,
      LEGACY_TELEMETRY_EVENT_OBJECT,
      slug,
      {
        reason,
      }
    );
    Glean.nimbusEvents.enrollFailed.record({
      experiment: slug,
      reason,
    });
  },

  recordEnrollmentStatus({
    slug,
    status,
    reason,
    branch,
    error_string,
    conflict_slug,
  }) {
    Glean.nimbusEvents.enrollmentStatus.record({
      slug,
      status,
      reason,
      branch,
      error_string,
      conflict_slug,
    });
  },

  recordExposure(slug, branchSlug, featureId) {
    Glean.normandy.exposeNimbusExperiment.record({
      value: slug,
      branchSlug,
      featureId,
    });
    Glean.nimbusEvents.exposure.record({
      experiment: slug,
      branch: branchSlug,
      feature_id: featureId,
    });
  },

  recordMigration(migration, error) {
    Glean.nimbusEvents.migration.record(
      Object.assign(
        {
          migration_id: migration,
          success: typeof error === "undefined",
        },
        typeof error !== "undefined" ? { error_reason: error } : {}
      )
    );
  },

  recordUnenrollment(
    slug,
    reason,
    branchSlug,
    { changedPref, conflictingSlug, prefType, prefName } = {}
  ) {
    lazy.TelemetryEnvironment.setExperimentInactive(slug);
    Services.fog.setExperimentInactive(slug);

    lazy.TelemetryEvents.sendEvent(
      LegacyTelemetryEvents.UNENROLL,
      LEGACY_TELEMETRY_EVENT_OBJECT,
      slug,
      Object.assign(
        {
          reason,
          branch: branchSlug,
        },
        reason === UnenrollReason.CHANGED_PREF
          ? { changedPref: changedPref.name }
          : {},
        reason === UnenrollReason.PREF_FLIPS_CONFLICT
          ? { conflictingSlug }
          : {},
        reason === UnenrollReason.PREF_FLIPS_FAILED
          ? { prefType, prefName }
          : {}
      )
    );

    Glean.nimbusEvents.unenrollment.record(
      Object.assign(
        {
          experiment: slug,
          branch: branchSlug,
          reason,
        },
        reason === UnenrollReason.CHANGED_PREF
          ? { changed_pref: changedPref.name }
          : {},
        reason === UnenrollReason.PREF_FLIPS_CONFLICT
          ? { conflicting_slug: conflictingSlug }
          : {},
        reason === UnenrollReason.PREF_FLIPS_FAILED
          ? {
              pref_type: prefType,
              pref_name: prefName,
            }
          : {}
      )
    );
  },

  recordUnenrollmentFailure(slug, reason) {
    lazy.TelemetryEvents.sendEvent(
      LegacyTelemetryEvents.UNENROLL_FAILED,
      LEGACY_TELEMETRY_EVENT_OBJECT,
      slug,
      { reason }
    );
    Glean.nimbusEvents.unenrollFailed.record({
      experiment: slug,
      reason,
    });
  },

  setExperimentActive(enrollment) {
    const type = `${EXPERIMENT_ACTIVE_PREFIX}${enrollment.experimentType}`;
    lazy.TelemetryEnvironment.setExperimentActive(
      enrollment.slug,
      enrollment.branch.slug,
      { type }
    );

    Services.fog.setExperimentActive(enrollment.slug, enrollment.branch.slug, {
      type,
    });
  },

  recordValidationFailure(
    slug,
    reason,
    { branch, locale, l10nIds: l10n_ids, featureIds: feature_ids } = {}
  ) {
    // Do not record invalid feature telemetry.
    if (reason === ValidationFailureReason.INVALID_FEATURE) {
      return;
    }

    const extra = Object.assign(
      { reason },
      reason === ValidationFailureReason.INVALID_BRANCH ? { branch } : {},
      reason === ValidationFailureReason.L10N_MISSING_ENTRY
        ? { l10n_ids, locale }
        : {},
      reason === ValidationFailureReason.L10N_MISSING_LOCALE ? { locale } : {},
      reason === ValidationFailureReason.UNSUPPORTED_FEATURES
        ? { feature_ids }
        : {}
    );

    lazy.TelemetryEvents.sendEvent(
      LegacyTelemetryEvents.VALIDATION_FAILED,
      LEGACY_TELEMETRY_EVENT_OBJECT,
      slug,
      extra
    );

    Glean.nimbusEvents.validationFailed.record({
      experiment: slug,
      ...extra,
    });
  },
};
