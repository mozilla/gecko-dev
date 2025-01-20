/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.test.whenever
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mockito.Mock
import org.mockito.Mockito.mock
import org.mockito.MockitoAnnotations
import org.mozilla.fenix.components.fake.FakeMetricController
import org.mozilla.fenix.components.metrics.MetricServiceType

class MetricsServiceHelperTest {

    @Mock
    private lateinit var mockLogger: Logger

    @Mock
    private lateinit var mockAnalytics: Analytics

    private val fakeMetricController = FakeMetricController()

    @Before
    fun setup() {
        MockitoAnnotations.openMocks(this)
        whenever(mockAnalytics.metrics).thenReturn(fakeMetricController)
        whenever(mockAnalytics.crashFactCollector).thenReturn(mock())
    }

    @Test
    fun `when telemetry is not enabled, and marketing telemetry is not enabled, do not start data reporting`() {
        startMetricsIfEnabled(
            mockLogger,
            mockAnalytics,
            isTelemetryEnabled = false,
            isMarketingTelemetryEnabled = false,
            isDailyUsagePingEnabled = false,
        )
        assertFalse(fakeMetricController.startedServiceTypes.contains(MetricServiceType.Data))
    }

    @Test
    fun `when telemetry is not enabled, and marketing telemetry is enabled, do not start data reporting`() {
        startMetricsIfEnabled(
            mockLogger,
            mockAnalytics,
            isTelemetryEnabled = false,
            isMarketingTelemetryEnabled = true,
            isDailyUsagePingEnabled = false,
        )
        assertFalse(fakeMetricController.startedServiceTypes.contains(MetricServiceType.Data))
    }

    @Test
    fun `when telemetry is enabled, and marketing telemetry is not enabled, start data reporting`() {
        startMetricsIfEnabled(
            mockLogger,
            mockAnalytics,
            isTelemetryEnabled = true,
            isMarketingTelemetryEnabled = false,
            isDailyUsagePingEnabled = false,
        )
        assertTrue(fakeMetricController.startedServiceTypes.contains(MetricServiceType.Data))
    }

    @Test
    fun `when telemetry is enabled, and marketing telemetry is enabled, start data reporting`() {
        startMetricsIfEnabled(
            mockLogger,
            mockAnalytics,
            isTelemetryEnabled = true,
            isMarketingTelemetryEnabled = true,
            isDailyUsagePingEnabled = false,
        )
        assertTrue(fakeMetricController.startedServiceTypes.contains(MetricServiceType.Data))
    }

    @Test
    fun `when telemetry is not enabled, and marketing telemetry is not enabled, do not start marketing reporting`() {
        startMetricsIfEnabled(
            mockLogger,
            mockAnalytics,
            isTelemetryEnabled = false,
            isMarketingTelemetryEnabled = false,
            isDailyUsagePingEnabled = false,
        )
        assertFalse(fakeMetricController.startedServiceTypes.contains(MetricServiceType.Marketing))
    }

    @Test
    fun `when telemetry is enabled, but marketing telemetry is not enabled, do not start marketing reporting`() {
        startMetricsIfEnabled(
            mockLogger,
            mockAnalytics,
            isTelemetryEnabled = true,
            isMarketingTelemetryEnabled = false,
            isDailyUsagePingEnabled = false,
        )
        assertFalse(fakeMetricController.startedServiceTypes.contains(MetricServiceType.Marketing))
    }

    @Test
    fun `when telemetry is not enabled, and marketing telemetry is enabled, start marketing reporting`() {
        startMetricsIfEnabled(
            mockLogger,
            mockAnalytics,
            isTelemetryEnabled = false,
            isMarketingTelemetryEnabled = true,
            isDailyUsagePingEnabled = false,
        )
        assertTrue(fakeMetricController.startedServiceTypes.contains(MetricServiceType.Marketing))
    }

    @Test
    fun `when telemetry is enabled, and marketing telemetry is enabled, start marketing reporting`() {
        startMetricsIfEnabled(
            mockLogger,
            mockAnalytics,
            isTelemetryEnabled = true,
            isMarketingTelemetryEnabled = true,
            isDailyUsagePingEnabled = false,
        )
        assertTrue(fakeMetricController.startedServiceTypes.contains(MetricServiceType.Marketing))
    }

    @Test
    fun `when daily usage ping is not enabled, and other telemetry is enabled, do not start usage reporting`() {
        startMetricsIfEnabled(
            mockLogger,
            mockAnalytics,
            isTelemetryEnabled = true,
            isMarketingTelemetryEnabled = true,
            isDailyUsagePingEnabled = false,
        )
        assertFalse(fakeMetricController.startedServiceTypes.contains(MetricServiceType.UsageReporting))
    }

    @Test
    fun `when daily usage ping is enabled, and other telemetry is not enabled, start usage reporting`() {
        startMetricsIfEnabled(
            mockLogger,
            mockAnalytics,
            isTelemetryEnabled = false,
            isMarketingTelemetryEnabled = false,
            isDailyUsagePingEnabled = true,
        )
        assertTrue(fakeMetricController.startedServiceTypes.contains(MetricServiceType.UsageReporting))
    }

    @Test
    fun `when daily usage ping is enabled, and marketing telemetry is enabled, start usage reporting`() {
        startMetricsIfEnabled(
            mockLogger,
            mockAnalytics,
            isTelemetryEnabled = false,
            isMarketingTelemetryEnabled = true,
            isDailyUsagePingEnabled = true,
        )
        assertTrue(fakeMetricController.startedServiceTypes.contains(MetricServiceType.UsageReporting))
    }

    @Test
    fun `when all telemetry is enabled, start usage reporting`() {
        startMetricsIfEnabled(
            mockLogger,
            mockAnalytics,
            isTelemetryEnabled = true,
            isMarketingTelemetryEnabled = true,
            isDailyUsagePingEnabled = true,
        )
        assertTrue(fakeMetricController.startedServiceTypes.contains(MetricServiceType.UsageReporting))
    }
}
