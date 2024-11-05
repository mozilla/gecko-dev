/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.store

import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import org.mozilla.fenix.onboarding.view.OnboardingAddOn

/**
 * Value type that represents the state of an add-ons onboarding page.
 */
data class OnboardingAddOnsState(
    val addOns: List<OnboardingAddOn> = emptyList(),
    val installationInProcess: Boolean = false,
) : State

/**
 * [Action] implementation related to [OnboardingAddOnsStore].
 */
sealed interface OnboardingAddOnsAction : Action {

    /**
     * Triggered when the store is initialized.
     */
    data class Init(val addons: List<OnboardingAddOn> = emptyList()) : OnboardingAddOnsAction

    /**
     * Updates the status of the add-on with the provide [addOnId].
     */
    data class UpdateStatus(val addOnId: String, val status: OnboardingAddonStatus) :
        OnboardingAddOnsAction
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
 * A [Store] that holds the [OnboardingAddOnsState] for the add-ons boarding page and reduces [OnboardingAddOnsAction]s
 * dispatched to the store.
 */
class OnboardingAddOnsStore : Store<OnboardingAddOnsState, OnboardingAddOnsAction>(
    initialState = OnboardingAddOnsState(),
    reducer = ::reducer,
) {
    init {
        dispatch(OnboardingAddOnsAction.Init())
    }
}

private fun reducer(
    state: OnboardingAddOnsState,
    action: OnboardingAddOnsAction,
): OnboardingAddOnsState =
    when (action) {
        is OnboardingAddOnsAction.Init -> OnboardingAddOnsState(addOns = action.addons)
        is OnboardingAddOnsAction.UpdateStatus -> {
            val mutableAddonsList = state.addOns.toMutableList()
            val index = mutableAddonsList.indexOfFirst { it.id == action.addOnId }
            if (index != -1) {
                val updatedAddon = mutableAddonsList[index].copy(status = action.status)
                mutableAddonsList[index] = updatedAddon
                val installing = when (action.status) {
                    OnboardingAddonStatus.INSTALLED, OnboardingAddonStatus.NOT_INSTALLED -> false
                    OnboardingAddonStatus.INSTALLING -> true
                }
                OnboardingAddOnsState(addOns = mutableAddonsList, installationInProcess = installing)
            } else {
                state
            }
        }
    }
