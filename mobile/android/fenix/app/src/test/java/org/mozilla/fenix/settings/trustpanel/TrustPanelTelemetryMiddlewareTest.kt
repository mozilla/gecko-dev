/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.trustpanel

import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.robolectric.testContext
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.TrackingProtection
import org.mozilla.fenix.settings.trustpanel.middleware.TrustPanelTelemetryMiddleware
import org.mozilla.fenix.settings.trustpanel.store.TrustPanelAction
import org.mozilla.fenix.settings.trustpanel.store.TrustPanelState
import org.mozilla.fenix.settings.trustpanel.store.TrustPanelStore
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class TrustPanelTelemetryMiddlewareTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @Test
    fun `GIVEN tracking protection is enabled WHEN toggle tracking protection action is dispatched THEN record tracking protection exception added telemetry`() {
        val store = createStore(
            trustPanelState = TrustPanelState(
                isTrackingProtectionEnabled = true,
            ),
        )
        assertNull(TrackingProtection.exceptionAdded.testGetValue())

        store.dispatch(TrustPanelAction.ToggleTrackingProtection).joinBlocking()

        assertNotNull(TrackingProtection.exceptionAdded.testGetValue())
    }

    @Test
    fun `GIVEN tracking protection is disabled WHEN toggle tracking protection action is dispatched THEN do not record tracking protection exception added telemetry`() {
        val store = createStore(
            trustPanelState = TrustPanelState(
                isTrackingProtectionEnabled = false,
            ),
        )
        assertNull(TrackingProtection.exceptionAdded.testGetValue())

        store.dispatch(TrustPanelAction.ToggleTrackingProtection).joinBlocking()

        assertNull(TrackingProtection.exceptionAdded.testGetValue())
    }

    private fun createStore(
        trustPanelState: TrustPanelState = TrustPanelState(),
    ) = TrustPanelStore(
        initialState = trustPanelState,
        middleware = listOf(
            TrustPanelTelemetryMiddleware(),
        ),
    )
}
