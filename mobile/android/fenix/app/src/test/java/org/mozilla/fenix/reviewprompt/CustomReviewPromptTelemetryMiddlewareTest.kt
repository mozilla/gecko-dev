/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.reviewprompt

import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.robolectric.testContext
import mozilla.telemetry.glean.internal.RecordedEvent
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.CustomReviewPrompt
import org.mozilla.fenix.helpers.FenixGleanTestRule
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class) // For gleanTestRule
class CustomReviewPromptTelemetryMiddlewareTest {

    @get:Rule
    val gleanTestRule = FenixGleanTestRule(testContext)

    @Test
    fun `WHEN Displayed action is dispatched THEN promptDisplayed event is recorded`() {
        assertNull(CustomReviewPrompt.promptDisplayed.testGetValue())

        val store = createStore()
        store.dispatch(CustomReviewPromptAction.Displayed).joinBlocking()

        assertEventRecorded("prompt_displayed") {
            CustomReviewPrompt.promptDisplayed.testGetValue()
        }
    }

    @Test
    fun `WHEN PositivePrePromptButtonClicked is dispatched THEN positiveFeedbackClicked event is recorded`() {
        assertNull(CustomReviewPrompt.positiveFeedbackClicked.testGetValue())

        val store = createStore()
        store.dispatch(CustomReviewPromptAction.PositivePrePromptButtonClicked).joinBlocking()

        assertEventRecorded("positive_feedback_clicked") {
            CustomReviewPrompt.positiveFeedbackClicked.testGetValue()
        }
    }

    @Test
    fun `WHEN NegativePrePromptButtonClicked is dispatched THEN negativeFeedbackClicked event is recorded`() {
        assertNull(CustomReviewPrompt.negativeFeedbackClicked.testGetValue())

        val store = createStore()
        store.dispatch(CustomReviewPromptAction.NegativePrePromptButtonClicked).joinBlocking()

        assertEventRecorded("negative_feedback_clicked") {
            CustomReviewPrompt.negativeFeedbackClicked.testGetValue()
        }
    }

    @Test
    fun `WHEN RateButtonClicked is dispatched THEN rateOnPlayStoreClicked event is recorded`() {
        assertNull(CustomReviewPrompt.rateOnPlayStoreClicked.testGetValue())

        val store = createStore(CustomReviewPromptState.Rate)
        store.dispatch(CustomReviewPromptAction.RateButtonClicked).joinBlocking()

        assertEventRecorded("rate_on_play_store_clicked") {
            CustomReviewPrompt.rateOnPlayStoreClicked.testGetValue()
        }
    }

    @Test
    fun `WHEN LeaveFeedbackButtonClicked is dispatched THEN openMozillaConnectClicked event is recorded`() {
        assertNull(CustomReviewPrompt.openMozillaConnectClicked.testGetValue())

        val store = createStore(CustomReviewPromptState.Feedback)
        store.dispatch(CustomReviewPromptAction.LeaveFeedbackButtonClicked).joinBlocking()

        assertEventRecorded("open_mozilla_connect_clicked") {
            CustomReviewPrompt.openMozillaConnectClicked.testGetValue()
        }
    }

    @Test
    fun `WHEN DismissRequested is dispatched THEN promptDismissed event is recorded`() {
        assertNull(CustomReviewPrompt.promptDismissed.testGetValue())

        val store = createStore()
        store.dispatch(CustomReviewPromptAction.DismissRequested).joinBlocking()

        assertEventRecorded("prompt_dismissed") {
            CustomReviewPrompt.promptDismissed.testGetValue()
        }
    }

    private fun createStore(
        initialState: CustomReviewPromptState = CustomReviewPromptState.PrePrompt,
    ): CustomReviewPromptStore {
        return CustomReviewPromptStore(
            initialState = initialState,
            middleware = listOf(
                CustomReviewPromptTelemetryMiddleware(),
            ),
        )
    }

    private fun assertEventRecorded(
        expectedName: String,
        snapshotProvider: () -> List<RecordedEvent>?,
    ) {
        val snapshot = snapshotProvider()
        assertNotNull(snapshot)
        assertEquals(1, snapshot!!.size)
        assertEquals(expectedName, snapshot.single().name)
    }
}
