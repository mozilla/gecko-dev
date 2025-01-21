/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.components.metrics.fake.FakeGleanProfileId
import org.mozilla.fenix.components.metrics.fake.FakeGleanProfileIdStore
import org.mozilla.fenix.components.metrics.fake.FakeGleanUsageReporting
import org.mozilla.fenix.components.metrics.fake.FakeLifecycleEventObserver
import org.mozilla.fenix.components.metrics.fake.FakeLifecycleOwner

internal class GleanUsageReportingMetricsServiceTest {

    private val fakeLifecycleOwner = FakeLifecycleOwner()
    private val fakeLifecycleEventObserver = FakeLifecycleEventObserver()
    private val fakeGleanUsageReporting = FakeGleanUsageReporting()

    @Test
    fun `lifecycle is not initially observed`() {
        createGleanUsageReportingMetricsService()
        assertTrue(fakeLifecycleOwner.observers.isEmpty())
        assertNull(fakeLifecycleEventObserver.lastEvent)
    }

    @Test
    fun `when service is started, the lifecycle observer is added`() {
        val service = createGleanUsageReportingMetricsService()
        service.start()
        assertTrue(fakeLifecycleOwner.observers.contains(fakeLifecycleEventObserver))
    }

    @Test
    fun `when service is started then stopped, the lifecycle observer is removed`() {
        val service = createGleanUsageReportingMetricsService()
        service.start()
        service.stop()
        assertFalse(fakeLifecycleOwner.observers.contains(fakeLifecycleEventObserver))
    }

    @Test
    fun `after starting and stopping the service, no lifecycle observers remain`() {
        val service = createGleanUsageReportingMetricsService()
        service.start()
        service.stop()
        assertTrue(fakeLifecycleOwner.observers.isEmpty())
    }

    @Test
    fun `usage reporting is enabled on start`() {
        val service = createGleanUsageReportingMetricsService()
        assertNull(fakeGleanUsageReporting.lastEnabled)
        service.start()
        assertNotNull(fakeGleanUsageReporting.lastEnabled)
        assertTrue(fakeGleanUsageReporting.lastEnabled!!)
    }

    @Test
    fun `usage reporting is disabled on start`() {
        val service = createGleanUsageReportingMetricsService()
        assertNull(fakeGleanUsageReporting.lastEnabled)
        service.start()
        service.stop()
        assertNotNull(fakeGleanUsageReporting.lastEnabled)
        assertFalse(fakeGleanUsageReporting.lastEnabled!!)
    }

    @Test
    fun `when usage reporting is started, no data deletion request is sent`() {
        createGleanUsageReportingMetricsService().start()
        assertNull(fakeGleanUsageReporting.dataDeletionRequested)
    }

    @Test
    fun `when usage reporting is stopped, a data deletion request is sent`() {
        val service = createGleanUsageReportingMetricsService()
        service.start()
        service.stop()
        assertNotNull(fakeGleanUsageReporting.dataDeletionRequested)
        assertTrue(fakeGleanUsageReporting.dataDeletionRequested!!)
    }

    private fun createGleanUsageReportingMetricsService() = GleanUsageReportingMetricsService(
        lifecycleOwner = fakeLifecycleOwner,
        gleanUsageReportingLifecycleObserver = fakeLifecycleEventObserver,
        gleanUsageReporting = fakeGleanUsageReporting,
        gleanProfileId = FakeGleanProfileId(),
        gleanProfileIdStore = FakeGleanProfileIdStore(),
    )
}
