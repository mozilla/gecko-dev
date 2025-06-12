/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.distributions

import org.mozilla.fenix.GleanMetrics.Partnerships

/**
 * A tool for recording metrics that have to do with Distributions
 */
interface DistributionMetricsProvider {
    /**
     * Record the [Partnerships.dt001Detected] event
     */
    fun recordDt001Detected()

    /**
     * Record the [Partnerships.dt001LegacyDetected] event
     */
    fun recordDt001LegacyDetected()

    /**
     * Record the [Partnerships.dt002Detected] event
     */
    fun recordDt002Detected()

    /**
     * Record the [Partnerships.dt002LegacyDetected] event
     */
    fun recordDt002LegacyDetected()

    /**
     * Record the [Partnerships.dt003Detected] event
     */
    fun recordDt003Detected()

    /**
     * Record the [Partnerships.dt003LegacyDetected] event
     */
    fun recordDt003LegacyDetected()
}

/**
 * The default implementation of [DistributionMetricsProvider]
 */
class DefaultDistributionMetricsProvider : DistributionMetricsProvider {
    override fun recordDt001Detected() {
        Partnerships.dt001Detected.record()
    }

    override fun recordDt001LegacyDetected() {
        Partnerships.dt001LegacyDetected.record()
    }

    override fun recordDt002Detected() {
        Partnerships.dt002Detected.record()
    }

    override fun recordDt002LegacyDetected() {
        Partnerships.dt002LegacyDetected.record()
    }

    override fun recordDt003Detected() {
        Partnerships.dt003Detected.record()
    }

    override fun recordDt003LegacyDetected() {
        Partnerships.dt003LegacyDetected.record()
    }
}
