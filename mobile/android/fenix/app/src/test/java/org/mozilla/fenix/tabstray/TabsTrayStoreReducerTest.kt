/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import mozilla.components.browser.state.state.createTab
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.tabstray.syncedtabs.getFakeSyncedTabList

class TabsTrayStoreReducerTest {

    @Test
    fun `WHEN UpdateInactiveTabs THEN inactive tabs are added`() {
        val inactiveTabs = listOf(
            createTab("https://mozilla.org"),
        )
        val initialState = TabsTrayState()
        val expectedState = initialState.copy(inactiveTabs = inactiveTabs)

        val resultState = TabsTrayReducer.reduce(
            initialState,
            TabsTrayAction.UpdateInactiveTabs(inactiveTabs),
        )

        assertEquals(expectedState, resultState)
    }

    @Test
    fun `GIVEN a new value for inactiveTabsExpanded WHEN UpdateInactiveExpanded is called THEN update the current value`() {
        val initialState = TabsTrayState(
            inactiveTabsExpanded = true,
        )

        var updatedState = TabsTrayReducer.reduce(
            initialState,
            TabsTrayAction.UpdateInactiveExpanded(false),
        )
        assertFalse(updatedState.inactiveTabsExpanded)

        updatedState = TabsTrayReducer.reduce(updatedState, TabsTrayAction.UpdateInactiveExpanded(true))
        assertTrue(updatedState.inactiveTabsExpanded)
    }

    @Test
    fun `WHEN UpdateNormalTabs THEN normal tabs are added`() {
        val normalTabs = listOf(
            createTab("https://mozilla.org"),
        )
        val initialState = TabsTrayState()
        val expectedState = initialState.copy(normalTabs = normalTabs)

        val resultState = TabsTrayReducer.reduce(
            initialState,
            TabsTrayAction.UpdateNormalTabs(normalTabs),
        )

        assertEquals(expectedState, resultState)
    }

    @Test
    fun `WHEN UpdatePrivateTabs THEN private tabs are added`() {
        val privateTabs = listOf(
            createTab("https://mozilla.org", private = true),
        )
        val initialState = TabsTrayState()
        val expectedState = initialState.copy(privateTabs = privateTabs)

        val resultState = TabsTrayReducer.reduce(
            initialState,
            TabsTrayAction.UpdatePrivateTabs(privateTabs),
        )

        assertEquals(expectedState, resultState)
    }

    @Test
    fun `WHEN UpdateSyncedTabs THEN synced tabs are added`() {
        val syncedTabs = getFakeSyncedTabList()
        val initialState = TabsTrayState()
        val expectedState = initialState.copy(syncedTabs = syncedTabs)

        val resultState = TabsTrayReducer.reduce(
            initialState,
            TabsTrayAction.UpdateSyncedTabs(syncedTabs),
        )

        assertEquals(expectedState, resultState)
    }
}
