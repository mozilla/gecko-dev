/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import mozilla.components.support.base.log.logger.Logger
import org.mozilla.fenix.components.metrics.MetricServiceType

/**
 * Helper function to start metric services if they are enabled.
 *
 * @param logger the logger to send logs about metrics service start.
 * @param analytics [Analytics] component to be started if needed.
 * @param isTelemetryEnabled indicate if the telemetry metric should be started.
 * @param isMarketingTelemetryEnabled indicate if the marketing metric should be started.
 */
fun startMetricsIfEnabled(
    logger: Logger,
    analytics: Analytics,
    isTelemetryEnabled: Boolean,
    isMarketingTelemetryEnabled: Boolean,
) {
    if (isTelemetryEnabled) {
        analytics.metrics.start(MetricServiceType.Data)
        analytics.crashFactCollector.start()
        logger.info("Telemetry metrics service started")
    }

    if (isMarketingTelemetryEnabled) {
        analytics.metrics.start(MetricServiceType.Marketing)
        logger.info("Marketing metrics service started")
    }
}
