/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.reviewprompt

import kotlinx.coroutines.flow.MutableSharedFlow
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import mozilla.components.lib.state.UiStore
import org.mozilla.fenix.reviewprompt.ui.CustomReviewPrompt

/** [Store] for holding [CustomReviewPromptState] and applying [CustomReviewPromptAction]s. */
class CustomReviewPromptStore(
    initialState: CustomReviewPromptState,
    middleware: List<Middleware<CustomReviewPromptState, CustomReviewPromptAction>> = emptyList(),
) : UiStore<CustomReviewPromptState, CustomReviewPromptAction>(
    initialState = initialState,
    reducer = ::reduceCustomReviewPromptActions,
    middleware = middleware,
) {
    val navigationEvents = MutableSharedFlow<CustomReviewPromptNavigationEvent>()
}

/** Available steps the [CustomReviewPrompt] can be showing. */
enum class CustomReviewPromptState : State {
    /** Initial state with positive and negative buttons to rate the experience. */
    PrePrompt,

    /** Positive state with a button to leave a Play Store rating. */
    Rate,

    /** Negative state with a button to leave feedback. */
    Feedback,
}

/** [Action]s to dispatch to [CustomReviewPromptStore] to modify [CustomReviewPromptState]. */
sealed class CustomReviewPromptAction : Action {
    /** Dispatched when the accessibility affordance to dismiss the prompt is clicked. */
    data object DismissRequested : CustomReviewPromptAction()

    /** Dispatched when the negative button in the pre-prompt is clicked. */
    data object NegativePrePromptButtonClicked : CustomReviewPromptAction()

    /** Dispatched when the positive button in the pre-prompt is clicked. */
    data object PositivePrePromptButtonClicked : CustomReviewPromptAction()

    /** Dispatched when the rate on Play Store button is clicked. */
    data object RateButtonClicked : CustomReviewPromptAction()

    /** Dispatched when the leave feedback button is clicked. */
    data object LeaveFeedbackButtonClicked : CustomReviewPromptAction()
}

internal fun reduceCustomReviewPromptActions(
    state: CustomReviewPromptState,
    action: CustomReviewPromptAction,
): CustomReviewPromptState = when (action) {
    CustomReviewPromptAction.NegativePrePromptButtonClicked -> CustomReviewPromptState.Feedback
    CustomReviewPromptAction.PositivePrePromptButtonClicked -> CustomReviewPromptState.Rate
    CustomReviewPromptAction.DismissRequested -> state
    CustomReviewPromptAction.RateButtonClicked -> state
    CustomReviewPromptAction.LeaveFeedbackButtonClicked -> state
}
