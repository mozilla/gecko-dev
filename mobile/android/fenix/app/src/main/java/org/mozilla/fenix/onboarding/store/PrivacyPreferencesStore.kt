/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.store

import mozilla.components.lib.state.Action
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.State
import mozilla.components.lib.state.UiStore

/**
 * Represents the state of privacy preferences.
 *
 * @property crashReportingEnabled Whether automatic crash reporting is enabled.
 * @property usageDataEnabled Whether usage data collection is enabled.
 */
data class PrivacyPreferencesState(
    val crashReportingEnabled: Boolean = false,
    val usageDataEnabled: Boolean = true,
) : State

/**
 * [Action] implementation related to [PrivacyPreferencesState].
 */
sealed class PrivacyPreferencesAction : Action {
    /**
     * Dispatched when the store is initialized.
     */
    data object Init : PrivacyPreferencesAction()

    /**
     * [PrivacyPreferencesAction] to update the crash reporting preference value to [enabled].
     *
     * @property enabled Flag to indicate whether the crash reporting option is enabled.
     */
    data class CrashReportingPreferenceUpdatedTo(val enabled: Boolean) : PrivacyPreferencesAction()

    /**
     * [PrivacyPreferencesAction] to update the usage data preference value to [enabled].
     *
     * @property enabled Flag to indicate whether the usage data option is enabled.
     */
    data class UsageDataPreferenceUpdatedTo(val enabled: Boolean) : PrivacyPreferencesAction()

    /**
     * [PrivacyPreferencesAction] indicates the crash reporting option "learn more" link was used.
     */
    data object CrashReportingLearnMore : PrivacyPreferencesAction()

    /**
     * [PrivacyPreferencesAction] indicates the usage data option "learn more" link was used.
     */
    data object UsageDataUserLearnMore : PrivacyPreferencesAction()
}

/**
 * Reducer for [PrivacyPreferencesStore].
 */
internal object PrivacyPreferencesReducer {
    fun reduce(
        state: PrivacyPreferencesState,
        action: PrivacyPreferencesAction,
    ): PrivacyPreferencesState {
        return when (action) {
            is PrivacyPreferencesAction.Init,
            is PrivacyPreferencesAction.CrashReportingLearnMore,
            is PrivacyPreferencesAction.UsageDataUserLearnMore,
            -> state

            is PrivacyPreferencesAction.CrashReportingPreferenceUpdatedTo ->
                state.copy(crashReportingEnabled = action.enabled)

            is PrivacyPreferencesAction.UsageDataPreferenceUpdatedTo ->
                state.copy(usageDataEnabled = action.enabled)
        }
    }
}

/**
 * A [UiStore] that holds the [PrivacyPreferencesState] for the privacy preferences and reduces
 * [PrivacyPreferencesAction]s dispatched to the store.
 */
class PrivacyPreferencesStore(
    initialState: PrivacyPreferencesState = PrivacyPreferencesState(),
    middlewares: List<Middleware<PrivacyPreferencesState, PrivacyPreferencesAction>> = emptyList(),
) : UiStore<PrivacyPreferencesState, PrivacyPreferencesAction>(
    initialState,
    PrivacyPreferencesReducer::reduce,
    middlewares,
) {
    init {
        dispatch(PrivacyPreferencesAction.Init)
    }
}
