/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.store

import android.graphics.Color
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.browser.store.BrowserScreenAction.CancelPrivateDownloadsOnPrivateTabsClosedAccepted
import org.mozilla.fenix.browser.store.BrowserScreenAction.ClosingLastPrivateTab
import org.mozilla.fenix.browser.store.BrowserScreenAction.CustomTabColorsUpdated
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class BrowserScreenStoreReducerTest {
    @Test
    fun `WHEN closing the last private tab THEN reset the state of accepting the risks`() {
        val browserScreenStore = BrowserScreenStore(
            BrowserScreenState(
                cancelPrivateDownloadsAccepted = true,
            ),
        )

        browserScreenStore.dispatch(ClosingLastPrivateTab("tabId", 1))

        assertFalse(browserScreenStore.state.cancelPrivateDownloadsAccepted)
    }

    @Test
    fun `WHEN accepting that all private downloads will be cancelled THEN update the state`() {
        val browserScreenStore = BrowserScreenStore(
            BrowserScreenState(
                cancelPrivateDownloadsAccepted = false,
            ),
        )

        browserScreenStore.dispatch(CancelPrivateDownloadsOnPrivateTabsClosedAccepted)

        assertTrue(browserScreenStore.state.cancelPrivateDownloadsAccepted)
    }

    @Test
    fun `WHEN custom tab colors are updated THEN update the state`() {
        val customColorsUpdate = CustomTabColors(
            toolbarColor = Color.RED,
            systemBarsColor = Color.BLUE,
            navigationBarDividerColor = Color.GREEN,
            readableColor = Color.BLACK,
        )
        val browserScreenStore = BrowserScreenStore()

        browserScreenStore.dispatch(CustomTabColorsUpdated(customColorsUpdate))

        assertEquals(customColorsUpdate, browserScreenStore.state.customTabColors)
    }
}
