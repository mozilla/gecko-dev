/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import androidx.annotation.VisibleForTesting

/**
 * Interface encapsulates the "Sent from Firefox" nimbus experiment.
 */
interface SentFromFirefoxFeature {
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
 * Default implementation of [SentFromFirefoxFeature].
 *
 * @property isFeatureEnabled Determines whether the "Sent from" feature is enabled.
 * @property templateMessage The template for the modified message.
 * @property appName The name of the application (Firefox).
 * @property downloadLink The link to download Firefox.
 */
class DefaultSentFromFirefoxFeature(
    val isFeatureEnabled: Boolean = false,
    val templateMessage: String = "",
    val appName: String = "",
    val downloadLink: String = "",
) : SentFromFirefoxFeature {

    override fun maybeAppendShareText(packageName: String, shareText: String) =
        if (packageName == WHATSAPP_PACKAGE_NAME && isFeatureEnabled) {
            getSentFromFirefoxMessage(shareText)
        } else {
            shareText
        }

    @VisibleForTesting
    internal fun getSentFromFirefoxMessage(sharedText: String) = String.format(
        templateMessage,
        sharedText,
        appName,
        downloadLink,
    )

    /** A helper object */
    companion object {
        const val WHATSAPP_PACKAGE_NAME = "com.whatsapp"
    }
}
