/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.app.Activity
import androidx.annotation.VisibleForTesting
import com.google.android.play.core.review.ReviewInfo
import com.google.android.play.core.review.ReviewManager
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.mozilla.fenix.GleanMetrics.ReviewPrompt
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/** Wraps the Play Store In-App Review API. */
class PlayStoreReviewPromptController(
    private val manager: ReviewManager,
    private val numberOfAppLaunches: () -> Int,
) {

    /** Launch the in-app review flow, unless we've hit the quota. */
    suspend fun tryPromptReview(activity: Activity) {
        val flow = withContext(Dispatchers.IO) { manager.requestReviewFlow() }

        flow.addOnCompleteListener {
            if (it.isSuccessful) {
                manager.launchReviewFlow(activity, it.result)
                recordReviewPromptEvent(
                    it.result.toString(),
                    numberOfAppLaunches(),
                    Date(),
                )
            }
        }
    }
}

/**
 * Records a [ReviewPrompt] with the required data.
 *
 * **Note:** The docs for [ReviewManager.launchReviewFlow] state 'In some circumstances the review
 * flow will not be shown to the user, e.g. they have already seen it recently, so do not assume that
 * calling this method will always display the review dialog.'
 * However, investigation has shown that a [ReviewInfo] instance with the flag:
 * - 'isNoOp=true' indicates that the prompt has NOT been displayed.
 * - 'isNoOp=false' indicates that a prompt has been displayed.
 * [ReviewManager.launchReviewFlow] will modify the ReviewInfo instance which can be used to determine
 * which of these flags is present.
 */
@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
fun recordReviewPromptEvent(
    reviewInfoAsString: String,
    numberOfAppLaunches: Int,
    now: Date,
) {
    val formattedLocalDatetime =
        SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss", Locale.getDefault()).format(now)

    // The internals of ReviewInfo cannot be accessed directly or cast nicely, so lets simply use
    // the object as a string.
    // ReviewInfo is susceptible to changes outside of our control hence the catch-all 'else' statement.
    val promptWasDisplayed = if (reviewInfoAsString.contains("isNoOp=true")) {
        "false"
    } else if (reviewInfoAsString.contains("isNoOp=false")) {
        "true"
    } else {
        "error"
    }

    ReviewPrompt.promptAttempt.record(
        ReviewPrompt.PromptAttemptExtra(
            promptWasDisplayed = promptWasDisplayed,
            localDatetime = formattedLocalDatetime,
            numberOfAppLaunches = numberOfAppLaunches,
        ),
    )
}
