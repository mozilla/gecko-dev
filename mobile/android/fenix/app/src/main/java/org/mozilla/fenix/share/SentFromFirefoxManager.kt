/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import androidx.annotation.VisibleForTesting

private const val WHATSAPP_PACKAGE_NAME = "com.whatsapp"

/**
 * Interface encapsulates the "Sent from Firefox" nimbus experiment.
 */
interface SentFromFirefoxManager {
    /**
     * Indicates whether the feature is enabled or not.
     */
    val featureEnabled: Boolean

    /**
     * Indicates whether the link sharing snackbar should be shown (once per install).
     */
    val shouldShowSnackbar: Boolean

    /**
     * Optionally appends a "Sent from Firefox" message to shared text.
     *
     * @param packageName The package name of the target application receiving the shared text.
     * @param shareText The original text being shared.
     * @return Either the modified share text including the "Sent from Firefox" message or the original.
     */
    fun maybeAppendShareText(packageName: String, shareText: String): String
}

/**
 * Default implementation of [SentFromFirefoxManager].
 *
 * @property snackbarEnabled Determines whether the "Sent from" snackbar is enabled.
 * @property templateMessage The template for the modified message.
 * @property appName The name of the application (Firefox).
 * @property downloadLink The link to download Firefox.
 * @property storage that persist the state of the link sharing snackbar
 */
class DefaultSentFromFirefoxManager(
    val snackbarEnabled: Boolean,
    val templateMessage: String,
    val appName: String,
    val downloadLink: String,
    val storage: SentFromStorage,
) : SentFromFirefoxManager {

    private var lastShareAppended: Boolean = false

    override val featureEnabled: Boolean
        get() = storage.featureEnabled

    override val shouldShowSnackbar: Boolean
        get() = featureEnabled && snackbarEnabled && !storage.isLinkSharingSettingsSnackbarShown &&
            lastShareAppended

    override fun maybeAppendShareText(packageName: String, shareText: String): String {
        val shouldAppendText = packageName == WHATSAPP_PACKAGE_NAME && featureEnabled

        // remembering state for possibly displaying a snackbar when the user returns to the app.
        lastShareAppended = shouldAppendText

        return if (shouldAppendText) {
            getSentFromFirefoxMessage(shareText)
        } else {
            shareText
        }
    }

    @VisibleForTesting
    internal fun getSentFromFirefoxMessage(sharedText: String) = String.format(
        templateMessage,
        sharedText,
        appName,
        downloadLink,
    )
}
