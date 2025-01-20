/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import junit.framework.TestCase.assertEquals
import junit.framework.TestCase.assertNull
import org.junit.Test
import org.mozilla.fenix.components.metrics.fake.FakeGleanProfileId
import org.mozilla.fenix.components.metrics.fake.FakeGleanProfileIdStore
import org.mozilla.fenix.components.metrics.fake.FakeGleanUsageReporting
import org.mozilla.fenix.components.metrics.fake.FakeLifecycleEventObserver
import org.mozilla.fenix.components.metrics.fake.FakeLifecycleOwner
import java.util.UUID

internal class GleanUserProfileIdTest {

    private val fakeGleanUsageId = FakeGleanProfileId()
    private val fakeGleanProfileIdStore = FakeGleanProfileIdStore()

    @Test
    fun `when the app is run for the first time, before initialising glean there are no stored profile ids`() {
        assertNull(fakeGleanProfileIdStore.appStoredProfileId)
        assertNull(fakeGleanUsageId.gleanStoredProfileId)
    }

    @Test
    fun `when the app is run for the first time, a new glean usage profile id is generated and set`() {
        createGleanUsageReportingMetricsService().start()
        assertEquals(fakeGleanUsageId.generatedProfileId, fakeGleanUsageId.gleanStoredProfileId)
    }

    @Test
    fun `when a new glean id is generated, it is stored in the app's prefs`() {
        createGleanUsageReportingMetricsService().start()
        assertEquals(fakeGleanUsageId.generatedProfileId.toString(), fakeGleanProfileIdStore.appStoredProfileId)
    }

    @Test
    fun `on the second run of the app, the glean usage profile id is retrieved from prefs`() {
        val service = createGleanUsageReportingMetricsService()
        val firstGeneratedProfileId = fakeGleanUsageId.generatedProfileId
        service.start()
        fakeGleanUsageId.generatedProfileId = UUID.randomUUID()
        service.start()
        assertEquals(firstGeneratedProfileId.toString(), fakeGleanProfileIdStore.appStoredProfileId)
    }

    @Test
    fun `on the second run of the app, the app stored glean usage profile id is set in glean`() {
        val service = createGleanUsageReportingMetricsService()
        val firstGeneratedProfileId = fakeGleanUsageId.generatedProfileId
        service.start()
        fakeGleanUsageId.generatedProfileId = UUID.randomUUID()
        service.start()
        assertEquals(firstGeneratedProfileId, fakeGleanUsageId.gleanStoredProfileId)
    }

    @Test
    fun `when glean telemetry is switched off, the glean usage profile id is unset`() {
        val service = createGleanUsageReportingMetricsService()
        service.start()
        service.stop()
        assertNull(fakeGleanProfileIdStore.appStoredProfileId)
        assertNull(fakeGleanUsageId.gleanStoredProfileId)
    }

    private fun createGleanUsageReportingMetricsService() = GleanUsageReportingMetricsService(
        lifecycleOwner = FakeLifecycleOwner(),
        gleanUsageReportingLifecycleObserver = FakeLifecycleEventObserver(),
        gleanUsageReporting = FakeGleanUsageReporting(),
        gleanProfileId = fakeGleanUsageId,
        gleanProfileIdStore = fakeGleanProfileIdStore,
    )
}
