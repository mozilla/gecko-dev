/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.store

import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import org.mozilla.fenix.onboarding.view.ToolbarOptionType

/**
 * Value type that represents the state of toolbar selection onboarding page.
 */
data class OnboardingToolbarState(val selected: ToolbarOptionType = ToolbarOptionType.TOOLBAR_TOP) :
    State

/**
 * [Action] implementation related to [OnboardingToolbarStore].
 */
sealed interface OnboardingToolbarAction : Action {

    /**
     * Triggered when the store is initialized.
     */
    data object Init : OnboardingToolbarAction

    /**
     * Updates the selected toolbar option to the given [selected] value.
     */
    data class UpdateSelected(val selected: ToolbarOptionType) : OnboardingToolbarAction
}

/**
 * A [Store] that holds the [OnboardingToolbarState] for the toolbar onboarding page and reduces
 * [OnboardingToolbarAction]s dispatched to the store.
 */
class OnboardingToolbarStore : Store<OnboardingToolbarState, OnboardingToolbarAction>(
    initialState = OnboardingToolbarState(),
    reducer = ::reducer,
) {
    init {
        dispatch(OnboardingToolbarAction.Init)
    }
}

private fun reducer(
    state: OnboardingToolbarState,
    action: OnboardingToolbarAction,
): OnboardingToolbarState =
    when (action) {
        is OnboardingToolbarAction.Init -> OnboardingToolbarState()
        is OnboardingToolbarAction.UpdateSelected -> state.copy(selected = action.selected)
    }
