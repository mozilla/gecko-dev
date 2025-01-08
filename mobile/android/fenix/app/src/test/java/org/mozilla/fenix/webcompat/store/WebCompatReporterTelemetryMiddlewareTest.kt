/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.webcompat.store

import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.Webcompatreporting
import org.mozilla.fenix.helpers.FenixGleanTestRule
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class WebCompatReporterTelemetryMiddlewareTest {
    @get:Rule
    val gleanTestRule = FenixGleanTestRule(testContext)

    @Test
    fun `WHEN dropdown value for reason has changed THEN record reason dropdown telemetry`() {
        val store = createStore()
        assertNull(Webcompatreporting.reasonDropdown.testGetValue())

        store.dispatch(WebCompatReporterAction.ReasonChanged(WebCompatReporterState.BrokenSiteReason.Media)).joinBlocking()

        assertNotNull(Webcompatreporting.reasonDropdown.testGetValue())
        val snapshot = Webcompatreporting.reasonDropdown.testGetValue()!!
        assertEquals(WebCompatReporterState.BrokenSiteReason.Media.name, snapshot)
    }

    @Test
    fun `WHEN send more info button is clicked THEN record send more info button telemetry`() {
        val store = createStore()
        assertNull(Webcompatreporting.sendMoreInfo.testGetValue())

        store.dispatch(WebCompatReporterAction.SendMoreInfoClicked).joinBlocking()

        val snapshot = Webcompatreporting.sendMoreInfo.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("send_more_info", snapshot.single().name)
    }

    @Test
    fun `WHEN send report button is clicked THEN record send report button telemetry`() {
        val store = createStore()
        assertNull(Webcompatreporting.send.testGetValue())

        store.dispatch(WebCompatReporterAction.SendReportClicked).joinBlocking()

        val snapshot = Webcompatreporting.send.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("send", snapshot.single().name)
    }

    private fun createStore(
        webCompatReporterState: WebCompatReporterState = WebCompatReporterState(),
    ) = WebCompatReporterStore(
        initialState = webCompatReporterState,
        middleware = listOf(
            WebCompatReporterTelemetryMiddleware(),
        ),
    )
}
