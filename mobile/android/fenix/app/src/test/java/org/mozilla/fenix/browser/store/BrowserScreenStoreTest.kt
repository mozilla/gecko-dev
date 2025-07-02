/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.store

import androidx.lifecycle.Lifecycle
import androidx.test.ext.junit.runners.AndroidJUnit4
import io.mockk.mockk
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.browser.store.BrowserScreenAction.CancelPrivateDownloadsOnPrivateTabsClosedAccepted
import org.mozilla.fenix.browser.store.BrowserScreenAction.ClosingLastPrivateTab
import org.mozilla.fenix.browser.store.BrowserScreenAction.EnvironmentRehydrated
import org.mozilla.fenix.browser.store.BrowserScreenStore.Environment
import org.mozilla.fenix.helpers.lifecycle.TestLifecycleOwner

@RunWith(AndroidJUnit4::class)
class BrowserScreenStoreTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val lifecycleOwner = TestLifecycleOwner(Lifecycle.State.RESUMED)

    @Test
    fun `WHEN closing the last private tab THEN remember this in state`() {
        val store = buildStore(true)

        store.dispatch(ClosingLastPrivateTab("tabId", 2))

        assertFalse(store.state.cancelPrivateDownloadsAccepted)
    }

    @Test
    fun `WHEN accepting to cancel private downloads on closing the last private tab THEN remember this in state`() {
        val store = buildStore(false)

        store.dispatch(CancelPrivateDownloadsOnPrivateTabsClosedAccepted)

        assertTrue(store.state.cancelPrivateDownloadsAccepted)
    }

    private fun buildStore(
        cancelPrivateDownloadsAccepted: Boolean = false,
    ) = BrowserScreenStore(
        initialState = BrowserScreenState(
            cancelPrivateDownloadsAccepted = cancelPrivateDownloadsAccepted,
        ),
    ).also {
        it.dispatch(EnvironmentRehydrated(Environment(testContext, lifecycleOwner, mockk())))
    }
}
