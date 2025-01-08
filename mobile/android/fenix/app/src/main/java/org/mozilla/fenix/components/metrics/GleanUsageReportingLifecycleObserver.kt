/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.LifecycleOwner
import org.mozilla.fenix.components.metrics.GleanUsageReportingApi.UsageReason.ACTIVE
import org.mozilla.fenix.components.metrics.GleanUsageReportingApi.UsageReason.INACTIVE

internal class GleanUsageReportingLifecycleObserver(
    private val gleanUsageReportingApi: GleanUsageReportingApi = GleanUsageReporting(),
    private val currentTimeProvider: () -> Long = { System.currentTimeMillis() },
) : LifecycleEventObserver {

    private var durationStartMs: Long? = null

    /**
     * Called when lifecycle events are triggered.
     */
    override fun onStateChanged(source: LifecycleOwner, event: Lifecycle.Event) {
        when (event) {
            Lifecycle.Event.ON_START -> {
                durationStartMs = currentTimeProvider()
                with(gleanUsageReportingApi) {
                    setUsageReason(ACTIVE)
                    submitPing()
                }
            }
            Lifecycle.Event.ON_STOP -> {
                with(gleanUsageReportingApi) {
                    val lastDurationStartMs = durationStartMs
                    lastDurationStartMs?.also {
                        setDuration(currentTimeProvider() - lastDurationStartMs)
                    }
                    setUsageReason(INACTIVE)
                    submitPing()
                }
            }
            else -> {
                // For other lifecycle events, do nothing
            }
        }
    }
}
