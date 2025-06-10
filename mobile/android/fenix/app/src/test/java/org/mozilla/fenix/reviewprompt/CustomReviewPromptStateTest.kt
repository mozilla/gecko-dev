/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.reviewprompt

import org.junit.Assert.assertEquals
import org.junit.Test

class CustomReviewPromptStateTest {
    @Test
    fun `WHEN positive button is clicked in the pre prompt THEN goes to the rate step`() {
        val initialState = CustomReviewPromptState.PrePrompt

        val updatedState = reduceCustomReviewPromptActions(
            initialState,
            CustomReviewPromptAction.PositivePrePromptButtonClicked,
        )

        assertEquals(CustomReviewPromptState.Rate, updatedState)
    }

    @Test
    fun `WHEN negative button is clicked in the pre prompt THEN goes to the feedback step`() {
        val initialState = CustomReviewPromptState.PrePrompt

        val updatedState = reduceCustomReviewPromptActions(
            initialState,
            CustomReviewPromptAction.NegativePrePromptButtonClicked,
        )

        assertEquals(CustomReviewPromptState.Feedback, updatedState)
    }

    @Test
    fun `WHEN other actions are dispatched THEN doesn't change state`() {
        val initialState = CustomReviewPromptState.PrePrompt

        assertEquals(
            initialState,
            reduceCustomReviewPromptActions(
                initialState,
                CustomReviewPromptAction.DismissRequested,
            ),
        )
        assertEquals(
            initialState,
            reduceCustomReviewPromptActions(
                initialState,
                CustomReviewPromptAction.RateButtonClicked,
            ),
        )
        assertEquals(
            initialState,
            reduceCustomReviewPromptActions(
                initialState,
                CustomReviewPromptAction.LeaveFeedbackButtonClicked,
            ),
        )
    }
}
