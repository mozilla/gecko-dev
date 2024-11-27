/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.telemetry

import android.content.Context
import android.content.res.Resources
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.search.telemetry.SearchProviderModel
import mozilla.components.feature.search.telemetry.ads.AdsTelemetry
import mozilla.components.feature.search.telemetry.incontent.InContentTelemetry
import org.junit.Test
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.mock
import org.mockito.Mockito.verify
import org.mozilla.focus.Components

class GleanMetricsServiceTest {
    @Test
    fun `WHEN installSearchTelemetryExtensions is called THEN install the ads and search telemetry extensions`() {
        val components = mock(Components::class.java)
        val store = mock(BrowserStore::class.java)
        val engine = mock(Engine::class.java)
        val adsExtension = mock(AdsTelemetry::class.java)
        val searchExtension = mock(InContentTelemetry::class.java)
        val providerList: List<SearchProviderModel> = mock()
        val context = mock(Context::class.java)
        val resources = mock(Resources::class.java)
        doReturn(engine).`when`(components).engine
        doReturn(store).`when`(components).store
        doReturn(adsExtension).`when`(components).adsTelemetry
        doReturn(searchExtension).`when`(components).searchTelemetry
        doReturn(resources).`when`(context).resources
        doReturn("").`when`(resources).getString(anyInt())
        val glean = GleanMetricsService(context)

        runBlocking {
            glean.installSearchTelemetryExtensions(components, providerList)

            verify(adsExtension).install(engine, store, providerList)
            verify(searchExtension).install(engine, store, providerList)
        }
    }
}
