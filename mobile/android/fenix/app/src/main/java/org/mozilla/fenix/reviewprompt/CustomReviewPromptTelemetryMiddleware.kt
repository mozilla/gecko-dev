/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.reviewprompt

import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.CustomReviewPrompt

internal class CustomReviewPromptTelemetryMiddleware :
    Middleware<CustomReviewPromptState, CustomReviewPromptAction> {

    override fun invoke(
        context: MiddlewareContext<CustomReviewPromptState, CustomReviewPromptAction>,
        next: (CustomReviewPromptAction) -> Unit,
        action: CustomReviewPromptAction,
    ) {
        next(action)

        when (action) {
            CustomReviewPromptAction.Displayed -> {
                CustomReviewPrompt.promptDisplayed.record(NoExtras())
            }

            CustomReviewPromptAction.PositivePrePromptButtonClicked -> {
                CustomReviewPrompt.positiveFeedbackClicked.record(NoExtras())
            }

            CustomReviewPromptAction.NegativePrePromptButtonClicked -> {
                CustomReviewPrompt.negativeFeedbackClicked.record(NoExtras())
            }

            CustomReviewPromptAction.RateButtonClicked -> {
                CustomReviewPrompt.rateOnPlayStoreClicked.record(NoExtras())
            }

            CustomReviewPromptAction.LeaveFeedbackButtonClicked -> {
                CustomReviewPrompt.openMozillaConnectClicked.record(NoExtras())
            }

            CustomReviewPromptAction.DismissRequested -> {
                CustomReviewPrompt.promptDismissed.record(NoExtras())
            }
        }
    }
}
