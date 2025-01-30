/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.store

import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext

/**
 * [Middleware] that reacts to various [PrivacyPreferencesAction]s and updates any corresponding preferences.
 *
 * @param privacyPreferencesRepository [PrivacyPreferencesRepository] used to access the privacy preferences.
 */
class PrivacyPreferencesMiddleware(
    private val privacyPreferencesRepository: PrivacyPreferencesRepository,
) : Middleware<PrivacyPreferencesState, PrivacyPreferencesAction> {

    override fun invoke(
        context: MiddlewareContext<PrivacyPreferencesState, PrivacyPreferencesAction>,
        next: (PrivacyPreferencesAction) -> Unit,
        action: PrivacyPreferencesAction,
    ) {
        next(action)

        when (action) {
            is PrivacyPreferencesAction.Init -> privacyPreferencesRepository.init()

            is PrivacyPreferencesAction.CrashReportingPreferenceUpdatedTo -> {
                privacyPreferencesRepository.updatePrivacyPreference(
                    PrivacyPreferencesRepository.PrivacyPreferenceUpdate(
                        preferenceType = PrivacyPreferencesRepository.PrivacyPreference.CrashReporting,
                        value = action.enabled,
                    ),
                )
            }

            is PrivacyPreferencesAction.UsageDataPreferenceUpdatedTo -> {
                privacyPreferencesRepository.updatePrivacyPreference(
                    PrivacyPreferencesRepository.PrivacyPreferenceUpdate(
                        preferenceType = PrivacyPreferencesRepository.PrivacyPreference.UsageData,
                        value = action.enabled,
                    ),
                )
            }

            // no-ops
            is PrivacyPreferencesAction.CrashReportingLearnMore,
            is PrivacyPreferencesAction.UsageDataUserLearnMore,
            -> {}
        }
    }
}
