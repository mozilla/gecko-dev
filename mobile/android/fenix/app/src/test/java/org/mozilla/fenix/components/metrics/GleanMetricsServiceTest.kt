/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.content.Context
import junit.framework.TestCase.assertEquals
import junit.framework.TestCase.assertNull
import mozilla.components.support.utils.RunWhenReadyQueue
import org.junit.Test
import org.mockito.Mockito.mock
import java.util.UUID

internal class GleanMetricsServiceTest {

    private val mockContext: Context = mock()
    private val fakeGleanUsageId = object : GleanProfileId {
        var gleanStoredProfileId: UUID? = null
        var generatedProfileId: UUID = UUID.randomUUID()

        override fun generateAndSet(): UUID {
            gleanStoredProfileId = generatedProfileId
            return generatedProfileId
        }
        override fun set(profileId: UUID) { gleanStoredProfileId = profileId }
        override fun unset() { gleanStoredProfileId = null }
    }
    private val fakeGleanProfileIdStore = object : GleanProfileIdStore {
        var appStoredProfileId: String? = null
        override var profileId: String?
            get() = appStoredProfileId
            set(value) { appStoredProfileId = value }

        override fun clear() {
            appStoredProfileId = null
        }
    }

    @Test
    fun `when the app is run for the first time, before initialising glean there are no stored profile ids`() {
        assertNull(fakeGleanProfileIdStore.appStoredProfileId)
        assertNull(fakeGleanUsageId.gleanStoredProfileId)
    }

    @Test
    fun `when the app is run for the first time, a new glean usage profile id is generated and set`() {
        createGleanMetricsService().start()
        assertEquals(fakeGleanUsageId.generatedProfileId, fakeGleanUsageId.gleanStoredProfileId)
    }

    @Test
    fun `when a new glean id is generated, it is stored in the app's prefs`() {
        createGleanMetricsService().start()
        assertEquals(fakeGleanUsageId.generatedProfileId.toString(), fakeGleanProfileIdStore.appStoredProfileId)
    }

    @Test
    fun `on the second run of the app, the glean usage profile id is retrieved from prefs`() {
        val service = createGleanMetricsService()
        val firstGeneratedProfileId = fakeGleanUsageId.generatedProfileId
        service.start()
        fakeGleanUsageId.generatedProfileId = UUID.randomUUID()
        service.start()
        assertEquals(firstGeneratedProfileId.toString(), fakeGleanProfileIdStore.appStoredProfileId)
    }

    @Test
    fun `on the second run of the app, the app stored glean usage profile id is set in glean`() {
        val service = createGleanMetricsService()
        val firstGeneratedProfileId = fakeGleanUsageId.generatedProfileId
        service.start()
        fakeGleanUsageId.generatedProfileId = UUID.randomUUID()
        service.start()
        assertEquals(firstGeneratedProfileId, fakeGleanUsageId.gleanStoredProfileId)
    }

    @Test
    fun `when glean telemetry is switched off, the glean usage profile id is unset`() {
        val service = createGleanMetricsService()
        service.start()
        service.stop()
        assertNull(fakeGleanProfileIdStore.appStoredProfileId)
        assertNull(fakeGleanUsageId.gleanStoredProfileId)
    }

    private fun createGleanMetricsService() =
        GleanMetricsService(
            mockContext,
            RunWhenReadyQueue(),
            fakeGleanUsageId,
            fakeGleanProfileIdStore,
        )
}
