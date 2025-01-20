/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.gleandebugtools

import mozilla.telemetry.glean.Glean

/**
 * A storage used to access the Glean APIs.
 */
interface GleanDebugToolsStorage {

    /**
     * Toggle whether to log pings to console.
     */
    fun setLogPings(enabled: Boolean)

    /**
     * Send a ping.
     *
     * @param pingType The type of ping to submit.
     * @param debugViewTag The debug tag to use for the ping.
     */
    fun sendPing(pingType: String, debugViewTag: String)
}

/**
 * The default storage, used by the [GleanDebugToolsMiddleware] to access the Glean APIs.
 */
class DefaultGleanDebugToolsStorage : GleanDebugToolsStorage {
    override fun setLogPings(enabled: Boolean) {
        Glean.setLogPings(enabled)
    }

    override fun sendPing(pingType: String, debugViewTag: String) {
        Glean.setDebugViewTag(debugViewTag)
        Glean.submitPingByName(pingType)
    }

    /**
     * @see [DefaultGleanDebugToolsStorage].
     */
    companion object {

        /**
         * Get all the types of pings that can be submitted.
         */
        fun getPingTypes(): Set<String> {
            return Glean.getRegisteredPingNames()
        }
    }
}
