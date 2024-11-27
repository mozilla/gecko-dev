/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.store

import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.GleanMetrics.Onboarding

/**
 * [Middleware] for recording telemetry based on [PrivacyPreferencesAction]s.
 */
class PrivacyPreferencesTelemetryMiddleware :
    Middleware<PrivacyPreferencesState, PrivacyPreferencesAction> {
    override fun invoke(
        context: MiddlewareContext<PrivacyPreferencesState, PrivacyPreferencesAction>,
        next: (PrivacyPreferencesAction) -> Unit,
        action: PrivacyPreferencesAction,
    ) {
        next(action)

        when (action) {
            is PrivacyPreferencesAction.CrashReportingPreferenceUpdatedTo ->
                Onboarding.privacyPreferencesModalCrashReportingEnabled.record(
                    Onboarding.PrivacyPreferencesModalCrashReportingEnabledExtra(action.enabled),
                )

            is PrivacyPreferencesAction.UsageDataPreferenceUpdatedTo ->
                Onboarding.privacyPreferencesModalUsageDataEnabled.record(
                    Onboarding.PrivacyPreferencesModalUsageDataEnabledExtra(action.enabled),
                )

            is PrivacyPreferencesAction.CrashReportingChecked ->
                Onboarding.privacyPreferencesModalCrashReportingChecked.record(
                    Onboarding.PrivacyPreferencesModalCrashReportingCheckedExtra(action.checked),
                )

            is PrivacyPreferencesAction.UsageDataUserChecked ->
                Onboarding.privacyPreferencesModalUsageDataChecked.record(
                    Onboarding.PrivacyPreferencesModalUsageDataCheckedExtra(action.checked),
                )

            // no-ops
            is PrivacyPreferencesAction.Init -> {}
        }
    }
}
