/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.util

import androidx.annotation.VisibleForTesting
import java.util.Date

/**
 * Helper class to identify if a website has shown many dialogs.
 *
 * @param maxSuccessiveDialogMillisLimit Maximum time required
 * between dialogs in seconds before not showing more dialog.
 */
class PromptAbuserDetector(
    private val maxSuccessiveDialogMillisLimit: Int = MAX_SUCCESSIVE_DIALOG_MILLIS_LIMIT,
) {

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    var jsAlertCount = 0

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    var lastDialogShownAt = Date()

    var shouldShowMoreDialogs = true
        private set

    /**
     * Updates internal state for alerts counts.
     */
    fun resetJSAlertAbuseState() {
        jsAlertCount = 0
        shouldShowMoreDialogs = true
    }

    /**
     * Updates internal state for last shown and count of dialogs.
     */
    fun updateJSDialogAbusedState() {
        if (!areDialogsAbusedByTime()) {
            jsAlertCount = 0
        }
        ++jsAlertCount
        lastDialogShownAt = Date()
    }

    /**
     * Indicates whether or not user wants to see more dialogs.
     */
    fun userWantsMoreDialogs(checkBox: Boolean) {
        shouldShowMoreDialogs = checkBox
    }

    /**
     * Indicates whether dialogs are being abused or not.
     */
    fun areDialogsBeingAbused(): Boolean {
        return validationsEnabled && (areDialogsAbusedByTime() || areDialogsAbusedByCount())
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @Suppress("UndocumentedPublicFunction") // this is visible only for tests
    fun now() = Date()

    private fun areDialogsAbusedByTime(): Boolean {
        return if (jsAlertCount == 0) {
            false
        } else {
            val now = now()
            val diffInMillis = now.time - lastDialogShownAt.time
            validationsEnabled && (diffInMillis < maxSuccessiveDialogMillisLimit)
        }
    }

    private fun areDialogsAbusedByCount(): Boolean {
        return validationsEnabled && (jsAlertCount > MAX_SUCCESSIVE_DIALOG_COUNT)
    }

    companion object {
        // Maximum number of successive dialogs before we prompt users to disable dialogs.
        internal const val MAX_SUCCESSIVE_DIALOG_COUNT: Int = 2

        // Minimum time required between dialogs in milliseconds before enabling the stop dialog.
        internal const val MAX_SUCCESSIVE_DIALOG_MILLIS_LIMIT: Int = 3000

        /**
         * Only use for testing purpose.
         */
        @VisibleForTesting
        var validationsEnabled = true
    }
}
