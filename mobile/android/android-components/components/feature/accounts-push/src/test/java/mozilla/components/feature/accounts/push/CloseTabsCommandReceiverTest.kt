/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts.push

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.sync.DeviceCommandIncoming
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

class CloseTabsCommandReceiverTest {
    @Test
    fun `GIVEN a command to close multiple URLs that are open in tabs WHEN the command is received THEN all tabs are closed AND the observer is notified`() {
        val urls = listOf(
            "https://mozilla.org",
            "https://getfirefox.com",
            "https://example.org",
            "https://getthunderbird.com",
        )
        val browserStore = BrowserStore(
            initialState = BrowserState(
                tabs = urls.map { createTab(it) },
            ),
        )
        val receiver = CloseTabsCommandReceiver(browserStore)
        val observer = mock<CloseTabsCommandReceiver.Observer>()
        receiver.register(observer)

        receiver.receive(DeviceCommandIncoming.TabsClosed(null, urls))

        browserStore.waitUntilIdle()

        assertTrue(browserStore.state.tabs.isEmpty())
        verify(observer).onTabsClosed(any())
        verify(observer, never()).onLastTabClosed()
    }

    @Test
    fun `GIVEN a command to close a URL that is not open in a tab WHEN the command is received THEN the observer is not notified`() {
        val browserStore = BrowserStore()
        val receiver = CloseTabsCommandReceiver(browserStore)
        val observer = mock<CloseTabsCommandReceiver.Observer>()
        receiver.register(observer)

        receiver.receive(DeviceCommandIncoming.TabsClosed(null, listOf("https://mozilla.org")))

        browserStore.waitUntilIdle()

        verify(observer, never()).onTabsClosed(any())
        verify(observer, never()).onLastTabClosed()
    }

    @Test
    fun `GIVEN a command to close a URL that is open in the currently selected tab WHEN the command is received THEN the tab is closed AND the observer is notified`() {
        val browserStore = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://getfirefox.com", id = "1"),
                ),
                selectedTabId = "1",
            ),
        )
        val processor = CloseTabsCommandReceiver(browserStore)
        val observer = mock<CloseTabsCommandReceiver.Observer>()
        processor.register(observer)

        processor.receive(DeviceCommandIncoming.TabsClosed(null, listOf("https://getfirefox.com")))

        browserStore.waitUntilIdle()

        assertTrue(browserStore.state.tabs.isEmpty())
        assertNull(browserStore.state.selectedTabId)
        verify(observer).onTabsClosed(eq(listOf("https://getfirefox.com")))
        verify(observer).onLastTabClosed()
    }

    @Test
    fun `GIVEN a command to close duplicate URLs that are open in tabs WHEN the command is received THEN the number of tabs closed matches the number of URLs AND the observer is notified`() {
        val browserStore = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://mozilla.org", id = "1"),
                    createTab("https://mozilla.org", id = "2"),
                    createTab("https://getfirefox.com", id = "3"),
                    createTab("https://getfirefox.com", id = "4"),
                    createTab("https://getfirefox.com", id = "5"),
                    createTab("https://getthunderbird.com", id = "6"),
                    createTab("https://example.org", id = "7"),
                ),
            ),
        )
        val processor = CloseTabsCommandReceiver(browserStore)
        val observer = mock<CloseTabsCommandReceiver.Observer>()
        processor.register(observer)

        processor.receive(
            DeviceCommandIncoming.TabsClosed(
                null,
                listOf(
                    "https://mozilla.org",
                    "https://getfirefox.com",
                    "https://getfirefox.com",
                    "https://example.org",
                    "https://example.org",
                ),
            ),
        )

        browserStore.waitUntilIdle()

        assertEquals(listOf("2", "5", "6"), browserStore.state.tabs.map { it.id })
        verify(observer).onTabsClosed(
            eq(
                listOf(
                    "https://mozilla.org",
                    "https://getfirefox.com",
                    "https://getfirefox.com",
                    "https://example.org",
                ),
            ),
        )
        verify(observer, never()).onLastTabClosed()
    }
}
