/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import io.mockk.mockk
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.robolectric.testContext
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.Metrics
import org.mozilla.fenix.GleanMetrics.TabsTray
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class) // for gleanTestRule
class TabsTrayTelemetryMiddlewareTest {

    private lateinit var store: TabsTrayStore
    private lateinit var tabsTrayTelemetryMiddleware: TabsTrayTelemetryMiddleware

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @Before
    fun setup() {
        tabsTrayTelemetryMiddleware = TabsTrayTelemetryMiddleware()
        store = TabsTrayStore(
            middlewares = listOf(tabsTrayTelemetryMiddleware),
            initialState = TabsTrayState(),
        )
    }

    @Test
    fun `WHEN inactive tabs are updated THEN report the count of inactive tabs`() {
        assertNull(TabsTray.hasInactiveTabs.testGetValue())
        assertNull(Metrics.inactiveTabsCount.testGetValue())

        store.dispatch(TabsTrayAction.UpdateInactiveTabs(emptyList()))
        store.waitUntilIdle()
        assertNotNull(TabsTray.hasInactiveTabs.testGetValue())
        assertNotNull(Metrics.inactiveTabsCount.testGetValue())
        assertEquals(0L, Metrics.inactiveTabsCount.testGetValue())
    }

    @Test
    fun `WHEN multi select mode from menu is entered THEN relevant metrics are collected`() {
        assertNull(TabsTray.enterMultiselectMode.testGetValue())

        store.dispatch(TabsTrayAction.EnterSelectMode)
        store.waitUntilIdle()

        assertNotNull(TabsTray.enterMultiselectMode.testGetValue())
        val snapshot = TabsTray.enterMultiselectMode.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("false", snapshot.single().extra?.getValue("tab_selected"))
    }

    @Test
    fun `WHEN multi select mode by long press is entered THEN relevant metrics are collected`() {
        store.dispatch(TabsTrayAction.AddSelectTab(mockk()))
        store.waitUntilIdle()

        assertNotNull(TabsTray.enterMultiselectMode.testGetValue())
        val snapshot = TabsTray.enterMultiselectMode.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("true", snapshot.single().extra?.getValue("tab_selected"))
    }

    @Test
    fun `WHEN the inactive tabs auto close dialog is shown THEN the metric is reported`() {
        assertNull(TabsTray.autoCloseSeen.testGetValue())

        store.dispatch(TabsTrayAction.TabAutoCloseDialogShown)
        store.waitUntilIdle()

        assertNotNull(TabsTray.autoCloseSeen.testGetValue())
    }

    @Test
    fun `WHEN the share all normal tabs button is clicked THEN the metric is reported`() {
        assertNull(TabsTray.shareAllTabs.testGetValue())

        store.dispatch(TabsTrayAction.ShareAllNormalTabs)
        store.waitUntilIdle()

        assertNotNull(TabsTray.shareAllTabs.testGetValue())
    }

    @Test
    fun `WHEN the share all private tabs button is clicked THEN the metric is reported`() {
        assertNull(TabsTray.shareAllTabs.testGetValue())

        store.dispatch(TabsTrayAction.ShareAllPrivateTabs)
        store.waitUntilIdle()

        assertNotNull(TabsTray.shareAllTabs.testGetValue())
    }

    @Test
    fun `WHEN the delete all normal tabs button is clicked THEN the metric is reported`() {
        assertNull(TabsTray.closeAllTabs.testGetValue())

        store.dispatch(TabsTrayAction.CloseAllNormalTabs)
        store.waitUntilIdle()

        assertNotNull(TabsTray.closeAllTabs.testGetValue())
    }

    @Test
    fun `WHEN the delete all private tabs button is clicked THEN the metric is reported`() {
        assertNull(TabsTray.closeAllTabs.testGetValue())

        store.dispatch(TabsTrayAction.CloseAllPrivateTabs)
        store.waitUntilIdle()

        assertNotNull(TabsTray.closeAllTabs.testGetValue())
    }
}
