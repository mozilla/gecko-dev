/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.reviewprompt

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.settings.SupportUtils

/**
 * [Middleware] that emits [CustomReviewPromptNavigationEvent]s handled by the hosting fragment
 * when an action results in navigation.
 */
class CustomReviewPromptNavigationMiddleware(
    private val scope: CoroutineScope,
) : Middleware<CustomReviewPromptState, CustomReviewPromptAction> {

    override fun invoke(
        context: MiddlewareContext<CustomReviewPromptState, CustomReviewPromptAction>,
        next: (CustomReviewPromptAction) -> Unit,
        action: CustomReviewPromptAction,
    ) {
        val events = (context.store as CustomReviewPromptStore).navigationEvents
        when (action) {
            CustomReviewPromptAction.DismissRequested -> {
                scope.launch {
                    events.emit(CustomReviewPromptNavigationEvent.Dismiss)
                }
            }

            CustomReviewPromptAction.RateButtonClicked -> {
                scope.launch {
                    events.emit(CustomReviewPromptNavigationEvent.OpenPlayStoreReviewPrompt)
                    events.emit(CustomReviewPromptNavigationEvent.Dismiss)
                }
            }

            CustomReviewPromptAction.LeaveFeedbackButtonClicked -> {
                scope.launch {
                    events.emit(CustomReviewPromptNavigationEvent.OpenNewTab(SupportUtils.ANDROID_SUPPORT_SUMO_URL))
                    events.emit(CustomReviewPromptNavigationEvent.Dismiss)
                }
            }

            CustomReviewPromptAction.NegativePrePromptButtonClicked -> {}
            CustomReviewPromptAction.PositivePrePromptButtonClicked -> {}
        }
        next(action)
    }
}

/** Events to emit to the fragment to handle navigation side-effects. */
sealed class CustomReviewPromptNavigationEvent {
    /** Dismiss the custom review prompt bottom sheet. */
    data object Dismiss : CustomReviewPromptNavigationEvent()

    /** Call the Play In-App Review API to show the review prompt. */
    data object OpenPlayStoreReviewPrompt : CustomReviewPromptNavigationEvent()

    /** Open the given [url] in a new tab. */
    data class OpenNewTab(val url: String) : CustomReviewPromptNavigationEvent()
}
