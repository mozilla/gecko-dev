/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  TelemetryEnvironment: "resource://gre/modules/TelemetryEnvironment.sys.mjs",
  TelemetryEvents: "resource://normandy/lib/TelemetryEvents.sys.mjs",

  REASON_PREFFLIPS_FAILED: "resource://nimbus/lib/PrefFlipsFeature.sys.mjs",
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

export const NimbusTelemetry = {
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
        reason === "changed-pref" ? { changedPref: changedPref.name } : {},
        reason === "prefFlips-conflict" ? { conflictingSlug } : {},
        reason === lazy.REASON_PREFFLIPS_FAILED ? { prefType, prefName } : {}
      )
    );

    Glean.nimbusEvents.unenrollment.record(
      Object.assign(
        {
          experiment: slug,
          branch: branchSlug,
          reason,
        },
        reason === "changed-pref" ? { changed_pref: changedPref.name } : {},
        reason === "prefFlips-conflict"
          ? { conflicting_slug: conflictingSlug }
          : {},
        reason === lazy.REASON_PREFFLIPS_FAILED
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
    { branch, feature, locale, l10nIds: l10n_ids } = {}
  ) {
    const extra = Object.assign(
      { reason },
      reason === "invalid-branch" ? { branch } : {},
      reason === "invalid-feature" ? { feature } : {},
      reason === "l10n-missing-locale" ? { locale } : {},
      reason === "l10n-missing-entry" ? { l10n_ids, locale } : {}
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
