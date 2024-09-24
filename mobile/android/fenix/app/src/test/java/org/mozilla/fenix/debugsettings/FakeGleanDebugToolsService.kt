package org.mozilla.fenix.debugsettings

import org.mozilla.fenix.debugsettings.gleandebugtools.GleanDebugToolsService

class FakeGleanDebugToolsService(
    var isSetLogPingsEnabled: Boolean = false,
) : GleanDebugToolsService {

    var baselinePingSent = false
    var metricsPingSent = false
    var pendingEventPingSent = false

    override fun setLogPings(enabled: Boolean) {
        isSetLogPingsEnabled = enabled
    }

    override fun sendBaselinePing(debugViewTag: String) {
        baselinePingSent = true
    }

    override fun sendMetricsPing(debugViewTag: String) {
        metricsPingSent = true
    }

    override fun sendPendingEventPing(debugViewTag: String) {
        pendingEventPingSent = true
    }
}
