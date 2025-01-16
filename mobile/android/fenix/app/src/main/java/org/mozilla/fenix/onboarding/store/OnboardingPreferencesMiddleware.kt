/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.store

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.onboarding.store.OnboardingPreferencesRepository.OnboardingPreference
import org.mozilla.fenix.onboarding.view.ThemeOptionType

/**
 * [Middleware] that reacts to various [OnboardingAction]s and updates any corresponding preferences.
 *
 * @param repository [OnboardingPreferencesRepository] used to access the relevant preferences.
 * @param coroutineScope The coroutine scope used for emitting flows.
 */
class OnboardingPreferencesMiddleware(
    private val repository: OnboardingPreferencesRepository,
    private val coroutineScope: CoroutineScope = CoroutineScope(Dispatchers.Main),
) : Middleware<OnboardingState, OnboardingAction> {
    override fun invoke(
        context: MiddlewareContext<OnboardingState, OnboardingAction>,
        next: (OnboardingAction) -> Unit,
        action: OnboardingAction,
    ) {
        next(action)

        when (action) {
            is OnboardingAction.Init -> {
                coroutineScope.launch {
                    repository.onboardingPreferenceUpdates
                        .collect { preferenceUpdate ->
                            if (preferenceUpdate.value) {
                                val updateAction =
                                    mapOnboardingPreferenceUpdateToStoreAction(preferenceUpdate)
                                context.store.dispatch(updateAction)
                            }
                        }
                }

                repository.init()
            }

            is OnboardingAction.OnboardingThemeAction.UpdateSelected -> {
                repository.updateOnboardingPreference(
                    OnboardingPreferencesRepository
                        .OnboardingPreferenceUpdate(action.selected.toOnboardingPreference()),
                )
            }

            // no-ops
            is OnboardingAction.OnboardingAddOnsAction.UpdateStatus,
            is OnboardingAction.OnboardingToolbarAction.UpdateSelected,
            -> {}
        }
    }

    private fun ThemeOptionType.toOnboardingPreference() = when (this) {
        ThemeOptionType.THEME_SYSTEM -> OnboardingPreference.DeviceTheme
        ThemeOptionType.THEME_LIGHT -> OnboardingPreference.LightTheme
        ThemeOptionType.THEME_DARK -> OnboardingPreference.DarkTheme
    }

    private fun mapOnboardingPreferenceUpdateToStoreAction(
        preferenceUpdate: OnboardingPreferencesRepository.OnboardingPreferenceUpdate,
    ): OnboardingAction {
        return when (preferenceUpdate.preferenceType) {
            OnboardingPreference.DeviceTheme ->
                OnboardingAction.OnboardingThemeAction.UpdateSelected(ThemeOptionType.THEME_SYSTEM)

            OnboardingPreference.LightTheme ->
                OnboardingAction.OnboardingThemeAction.UpdateSelected(ThemeOptionType.THEME_LIGHT)

            OnboardingPreference.DarkTheme ->
                OnboardingAction.OnboardingThemeAction.UpdateSelected(ThemeOptionType.THEME_DARK)
        }
    }
}
