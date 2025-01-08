/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.os.Build
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.GleanMetrics.Pings
import org.mozilla.fenix.GleanMetrics.Usage
import kotlin.time.DurationUnit
import kotlin.time.toDuration

/**
 * Makes the glean calls we need to manage the usage reporting ping.
 */
class GleanUsageReporting : GleanUsageReportingApi {

    override fun setDuration(durationMillis: Long) {
        val duration = durationMillis.toDuration(DurationUnit.MILLISECONDS)
        Usage.duration.setRawNanos(duration.inWholeNanoseconds)
    }

    override fun setUsageReason(usageReason: GleanUsageReportingApi.UsageReason) {
        Usage.reason.set(usageReason.name.lowercase())
    }

    override fun submitPing() {
        setUsageConstantValues()
        Pings.usageReporting.submit()
    }

    private fun setUsageConstantValues() {
        Usage.os.set("android")
        Usage.osVersion.set(Build.VERSION.RELEASE)
        Usage.appDisplayVersion.set(BuildConfig.VERSION_NAME)
        Usage.appChannel.set(BuildConfig.BUILD_TYPE)
    }
}
