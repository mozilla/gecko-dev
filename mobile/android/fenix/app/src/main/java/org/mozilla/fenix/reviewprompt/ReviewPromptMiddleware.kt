/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.reviewprompt

import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.components.ReviewPromptController.Companion.APPRX_MONTH_IN_MILLIS
import org.mozilla.fenix.components.ReviewPromptController.Companion.NUMBER_OF_LAUNCHES_REQUIRED
import org.mozilla.fenix.components.ReviewPromptController.Companion.NUMBER_OF_MONTHS_TO_PASS
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppAction.ReviewPromptAction.CheckIfEligibleForReviewPrompt
import org.mozilla.fenix.components.appstate.AppAction.ReviewPromptAction.DoNotShowReviewPrompt
import org.mozilla.fenix.components.appstate.AppAction.ReviewPromptAction.ReviewPromptShown
import org.mozilla.fenix.components.appstate.AppAction.ReviewPromptAction.ShowCustomReviewPrompt
import org.mozilla.fenix.components.appstate.AppAction.ReviewPromptAction.ShowPlayStorePrompt
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.utils.Settings

/**
 * [Middleware] checking if any of the triggers to show a review prompt is satisfied.
 */
class ReviewPromptMiddleware(
    private val settings: Settings,
    private val timeNowInMillis: () -> Long = { System.currentTimeMillis() },
) : Middleware<AppState, AppAction> {

    override fun invoke(
        context: MiddlewareContext<AppState, AppAction>,
        next: (AppAction) -> Unit,
        action: AppAction,
    ) {
        if (action !is AppAction.ReviewPromptAction) {
            next(action)
            return
        }

        when (action) {
            CheckIfEligibleForReviewPrompt -> handleReviewPromptCheck(context)
            ReviewPromptShown -> settings.lastReviewPromptTimeInMillis = timeNowInMillis()
            DoNotShowReviewPrompt -> Unit
            ShowCustomReviewPrompt -> Unit
            ShowPlayStorePrompt -> Unit
        }

        next(action)
    }

    private fun handleReviewPromptCheck(context: MiddlewareContext<AppState, AppAction>) {
        if (context.state.reviewPrompt != ReviewPromptState.Unknown) {
            // We only want to try to show it once to avoid unnecessary disk reads.
            return
        }

        if (!settings.isDefaultBrowser) {
            context.dispatch(DoNotShowReviewPrompt)
            return
        }

        val hasOpenedAtLeastFiveTimes = settings.numberOfAppLaunches >= NUMBER_OF_LAUNCHES_REQUIRED
        val now = timeNowInMillis()
        val approximatelyFourMonthsAgo = now - (APPRX_MONTH_IN_MILLIS * NUMBER_OF_MONTHS_TO_PASS)
        val lastPrompt = settings.lastReviewPromptTimeInMillis
        val hasNotBeenPromptedLastFourMonths =
            lastPrompt == 0L || lastPrompt <= approximatelyFourMonthsAgo

        if (hasOpenedAtLeastFiveTimes && hasNotBeenPromptedLastFourMonths) {
            if (settings.isTelemetryEnabled) {
                context.dispatch(ShowCustomReviewPrompt)
            } else {
                context.dispatch(ShowPlayStorePrompt)
            }
        } else {
            context.dispatch(DoNotShowReviewPrompt)
        }
    }
}
