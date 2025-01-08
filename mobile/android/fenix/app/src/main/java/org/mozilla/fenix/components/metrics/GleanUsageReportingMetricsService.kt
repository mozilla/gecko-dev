/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.ProcessLifecycleOwner
import mozilla.components.support.base.log.logger.Logger

/**
 * Metrics service which encapsulates sending the usage reporting ping.
 * @param lifecycleOwner the top level container whose lifecycle is followed by usage data
 * @param gleanUsageReportingLifecycleObserver this can be provided to control
 * the start / stop sending events for the usage reporting ping
 */
class GleanUsageReportingMetricsService(
    private val lifecycleOwner: LifecycleOwner = ProcessLifecycleOwner.get(),
    private val gleanUsageReportingLifecycleObserver: LifecycleEventObserver = GleanUsageReportingLifecycleObserver(),
) : MetricsService {

    override val type: MetricServiceType = MetricServiceType.UsageReporting
    private val logger = Logger("GleanUsageReportingMetricsService")

    override fun start() {
        logger.debug("Start GleanUsageReportingMetricsService")
        lifecycleOwner.lifecycle.addObserver(gleanUsageReportingLifecycleObserver)
    }

    override fun stop() {
        logger.debug("Stop GleanUsageReportingMetricsService")
        lifecycleOwner.lifecycle.removeObserver(gleanUsageReportingLifecycleObserver)
    }

    override fun track(event: Event) {
        // no-op
    }

    override fun shouldTrack(event: Event): Boolean = false
}
