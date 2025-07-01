/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.reviewprompt

import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.components.appstate.AppAction.ReviewPromptAction
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.reviewprompt.ReviewPromptState.Eligible.Type

class ReviewPromptReducerTest {

    val initialState = AppState()

    @Test
    fun `WHEN the review prompt check hasn't run yet THEN eligibility state is unknown`() {
        assertEquals(
            ReviewPromptState.Unknown,
            initialState.reviewPrompt,
        )
    }

    @Test
    fun `WHEN show Play Store prompt THEN sets eligible for Play Store prompt`() {
        val updatedState = ReviewPromptReducer.reduce(
            initialState,
            ReviewPromptAction.ShowPlayStorePrompt,
        )

        assertEquals(
            AppState(reviewPrompt = ReviewPromptState.Eligible(Type.PlayStore)),
            updatedState,
        )
    }

    @Test
    fun `WHEN show custom prompt THEN sets eligible for custom prompt`() {
        val updatedState = ReviewPromptReducer.reduce(
            initialState,
            ReviewPromptAction.ShowCustomReviewPrompt,
        )

        assertEquals(
            AppState(reviewPrompt = ReviewPromptState.Eligible(Type.Custom)),
            updatedState,
        )
    }

    @Test
    fun `WHEN don't show prompt THEN sets not eligible`() {
        val updatedState = ReviewPromptReducer.reduce(
            initialState,
            ReviewPromptAction.DoNotShowReviewPrompt,
        )

        assertEquals(
            AppState(reviewPrompt = ReviewPromptState.NotEligible),
            updatedState,
        )
    }

    @Test
    fun `WHEN prompt has been shown THEN sets not eligible`() {
        val updatedState = ReviewPromptReducer.reduce(
            initialState,
            ReviewPromptAction.ReviewPromptShown,
        )

        assertEquals(
            AppState(reviewPrompt = ReviewPromptState.NotEligible),
            updatedState,
        )
    }
}
