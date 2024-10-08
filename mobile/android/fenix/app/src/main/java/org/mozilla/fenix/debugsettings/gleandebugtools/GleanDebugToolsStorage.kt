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
     * Send a baseline ping.
     */
    fun sendBaselinePing(debugViewTag: String)

    /**
     * Send a metrics ping.
     */
    fun sendMetricsPing(debugViewTag: String)

    /**
     * Send a pending event ping.
     */
    fun sendPendingEventPing(debugViewTag: String)
}

/**
 * The default storage, used by the [GleanDebugToolsMiddleware] to access the Glean APIs.
 */
class DefaultGleanDebugToolsStorage : GleanDebugToolsStorage {
    override fun setLogPings(enabled: Boolean) {
        Glean.setLogPings(enabled)
    }

    override fun sendBaselinePing(debugViewTag: String) {
        Glean.setDebugViewTag(debugViewTag)
        Glean.submitPingByName("baseline")
    }

    override fun sendMetricsPing(debugViewTag: String) {
        Glean.setDebugViewTag(debugViewTag)
        Glean.submitPingByName("metrics")
    }

    override fun sendPendingEventPing(debugViewTag: String) {
        Glean.setDebugViewTag(debugViewTag)
        Glean.submitPingByName("events")
    }
}
