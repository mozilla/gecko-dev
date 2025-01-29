/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.focus.telemetry.fake

import org.mozilla.focus.telemetry.GleanUsageReportingApi

/**
 * Allows tests to insert their own version of a GleanUsageReportingApi
 * and make assertions against it
 */
class FakeGleanUsageReporting : GleanUsageReportingApi {
    var pingSubmitCount: Int = 0
    var lastUsageReason: String? = null
    var lastDurationMillis: Long? = null
    var lastEnabled: Boolean? = null
    var dataDeletionRequested: Boolean? = null

    override fun setEnabled(enabled: Boolean) {
        lastEnabled = enabled
    }

    override fun requestDataDeletion() {
        dataDeletionRequested = true
    }

    override fun setUsageReason(usageReason: GleanUsageReportingApi.UsageReason) {
        this.lastUsageReason = usageReason.name.lowercase()
    }

    override fun submitPing() {
        pingSubmitCount++
    }

    override fun setDuration(durationMillis: Long) {
        lastDurationMillis = durationMillis
    }
}
