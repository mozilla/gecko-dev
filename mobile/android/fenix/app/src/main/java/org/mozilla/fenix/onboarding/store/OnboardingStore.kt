/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.store

import mozilla.components.lib.state.Action
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import org.mozilla.fenix.onboarding.view.OnboardingAddOn
import org.mozilla.fenix.onboarding.view.ThemeOptionType
import org.mozilla.fenix.onboarding.view.ToolbarOptionType

/**
 * [State] for the onboarding views.
 *
 * @property addOns optional list of of add-ons.
 * @property addOnInstallationInProcess whether an add-on is in the process of being installed.
 * @property toolbarOptionSelected the selected toolbar option.
 * @property themeOptionSelected the selected theme option.
 */
data class OnboardingState(
    val addOns: List<OnboardingAddOn> = emptyList(),
    val addOnInstallationInProcess: Boolean = false,
    val toolbarOptionSelected: ToolbarOptionType = ToolbarOptionType.TOOLBAR_TOP,
    val themeOptionSelected: ThemeOptionType = ThemeOptionType.THEME_SYSTEM,
) : State

/**
 * [Action] implementation related to [OnboardingStore].
 */
sealed interface OnboardingAction : Action {

    /**
     * Triggered when the store is initialized.
     */
    data class Init(val addons: List<OnboardingAddOn> = emptyList()) : OnboardingAction

    /**
     * [Action] implementation related to add-ons.
     */
    sealed interface OnboardingAddOnsAction : OnboardingAction {
        /**
         * Updates the status of the add-on with the provide [addOnId].
         */
        data class UpdateStatus(val addOnId: String, val status: OnboardingAddonStatus) :
            OnboardingAddOnsAction
    }

    /**
     * [Action] implementation related to toolbar selection.
     */
    sealed interface OnboardingToolbarAction : OnboardingAction {
        /**
         * Updates the selected toolbar option to the given [selected] value.
         */
        data class UpdateSelected(val selected: ToolbarOptionType) : OnboardingToolbarAction
    }

    /**
     * [Action] implementation related to theme selection.
     */
    sealed interface OnboardingThemeAction : OnboardingAction {
        /**
         * Updates the selected theme option to the given [selected] value.
         */
        data class UpdateSelected(val selected: ThemeOptionType) : OnboardingThemeAction
    }
}

/**
 * Installation status in which an [OnboardingAddOn] could be.
 */
enum class OnboardingAddonStatus {
    INSTALLED,
    INSTALLING,
    NOT_INSTALLED,
}

/**
 * A [Store] that holds the [OnboardingState] for the add-ons boarding page and reduces [OnboardingAction]s
 * dispatched to the store.
 */
class OnboardingStore(middleware: List<Middleware<OnboardingState, OnboardingAction>> = emptyList()) :
    Store<OnboardingState, OnboardingAction>(
        initialState = OnboardingState(),
        reducer = ::reducer,
        middleware = middleware,
    ) {
    init {
        dispatch(OnboardingAction.Init())
    }
}

private fun reducer(
    state: OnboardingState,
    action: OnboardingAction,
): OnboardingState =
    when (action) {
        is OnboardingAction.Init -> OnboardingState(addOns = action.addons)
        is OnboardingAction.OnboardingAddOnsAction.UpdateStatus -> {
            val mutableAddonsList = state.addOns.toMutableList()
            val index = mutableAddonsList.indexOfFirst { it.id == action.addOnId }
            if (index != -1) {
                val updatedAddon = mutableAddonsList[index].copy(status = action.status)
                mutableAddonsList[index] = updatedAddon
                val installing = when (action.status) {
                    OnboardingAddonStatus.INSTALLED, OnboardingAddonStatus.NOT_INSTALLED -> false
                    OnboardingAddonStatus.INSTALLING -> true
                }
                OnboardingState(
                    addOns = mutableAddonsList,
                    addOnInstallationInProcess = installing,
                )
            } else {
                state
            }
        }

        is OnboardingAction.OnboardingToolbarAction.UpdateSelected -> state.copy(
            toolbarOptionSelected = action.selected,
        )

        is OnboardingAction.OnboardingThemeAction.UpdateSelected -> state.copy(
            themeOptionSelected = action.selected,
        )
    }
