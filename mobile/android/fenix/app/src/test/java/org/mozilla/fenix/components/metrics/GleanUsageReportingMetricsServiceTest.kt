/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.components.metrics.fake.FakeLifecycleEventObserver
import org.mozilla.fenix.components.metrics.fake.FakeLifecycleOwner

class GleanUsageReportingMetricsServiceTest {

    private val fakeLifecycleOwner = FakeLifecycleOwner()
    private val fakeLifecycleEventObserver = FakeLifecycleEventObserver()

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

    private fun createGleanUsageReportingMetricsService() = GleanUsageReportingMetricsService(
        lifecycleOwner = fakeLifecycleOwner,
        gleanUsageReportingLifecycleObserver = fakeLifecycleEventObserver,
    )
}
