/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen.middleware

import android.content.Context
import android.content.Intent
import mozilla.components.feature.downloads.INTENT_EXTRA_DOWNLOAD_ID
import mozilla.components.support.base.log.logger.Logger

/**
 * Defines a contract for sending broadcast messages.
 * This interface abstracts the underlying broadcast mechanism,
 * allowing for different implementations
 * and facilitating testing by enabling mock implementations.
 */
interface BroadcastSender {
    /**
     * Sends a broadcast message with the specified action and download ID.
     *
     * @param downloadId The unique identifier of the download related to this broadcast.
     * @param action The action string for the Intent to be broadcast.
     */
    fun sendBroadcast(downloadId: String, action: String)
}

/**
 * A default implementation of [BroadcastSender] that uses the Android system's
 * standard broadcast mechanism ([Context.sendBroadcast]).
 *
 * @param context The Android [Context] used to send the broadcast and access
 *                application-specific information like the package name.
 */
class DefaultBroadcastSender(
    private val context: Context,
) : BroadcastSender {
    private val logger = Logger("DefaultBroadcastSender")

    override fun sendBroadcast(downloadId: String, action: String) {
        val intent = Intent(action).apply {
            setPackage(context.applicationContext.packageName)
            putExtra(INTENT_EXTRA_DOWNLOAD_ID, downloadId)
        }
        context.sendBroadcast(intent)
        logger.debug("Sent broadcast: ACTION=$action for Download ID=$downloadId")
    }
}
