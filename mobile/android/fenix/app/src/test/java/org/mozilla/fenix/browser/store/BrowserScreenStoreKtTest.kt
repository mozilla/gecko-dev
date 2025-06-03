/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.store

import mozilla.components.concept.engine.translate.Language
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.browser.PageTranslationStatus
import org.mozilla.fenix.browser.ReaderModeStatus
import org.mozilla.fenix.browser.store.BrowserScreenAction.CancelPrivateDownloadsOnPrivateTabsClosedAccepted
import org.mozilla.fenix.browser.store.BrowserScreenAction.ClosingLastPrivateTab
import org.mozilla.fenix.browser.store.BrowserScreenAction.PageTranslationStatusUpdated
import org.mozilla.fenix.browser.store.BrowserScreenAction.ReaderModeStatusUpdated
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class BrowserScreenStoreKtTest {
    @get:Rule
    val mainCoroutineRule = MainCoroutineRule()

    @Test
    fun `WHEN the last private tab is closed THEN reset whether cancelling all private downloads was accepted`() {
        val store = BrowserScreenStore()

        store.dispatch(ClosingLastPrivateTab("tabId", 2))

        assertFalse(store.state.cancelPrivateDownloadsAccepted)
    }

    @Test
    fun `WHEN cancelling all private downloads is accepted THEN remember this as state`() {
        val store = BrowserScreenStore(
            BrowserScreenState(cancelPrivateDownloadsAccepted = false),
        )

        store.dispatch(CancelPrivateDownloadsOnPrivateTabsClosedAccepted)

        assertTrue(store.state.cancelPrivateDownloadsAccepted)
    }

    @Test
    fun `WHEN the reader mode status of the current page changes THEN remember the new details in state`() {
        val store = BrowserScreenStore()

        store.dispatch(ReaderModeStatusUpdated(ReaderModeStatus(true, true)))

        assertTrue(store.state.readerModeStatus.isAvailable)
        assertTrue(store.state.readerModeStatus.isActive)
    }

    @Test
    fun `WHEN the page translations status of the current page changes THEN remmeber the new details in state`() {
        val store = BrowserScreenStore()
        val expectedNewStatus = PageTranslationStatus(
            isTranslationPossible = true,
            isTranslated = false,
            isTranslateProcessing = true,
            fromSelectedLanguage = Language("fr"),
            toSelectedLanguage = Language("ro"),
        )

        store.dispatch(
            PageTranslationStatusUpdated(expectedNewStatus),
        )

        assertEquals(expectedNewStatus, store.state.pageTranslationStatus)
    }
}
