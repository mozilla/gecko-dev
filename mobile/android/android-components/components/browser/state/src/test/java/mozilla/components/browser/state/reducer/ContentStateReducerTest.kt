/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ContentStateReducerTest {
    @Test
    fun `updateContentState will return a new BrowserState with updated ContentState`() {
        val initialContentState = ContentState("emptyStateUrl")
        val browserState =
            BrowserState(tabs = listOf(TabSessionState("tabId", initialContentState)))

        val result = updateContentState(browserState, "tabId") { it.copy(url = "updatedUrl") }

        assertFalse(browserState == result)
        assertEquals("updatedUrl", result.tabs[0].content.url)
    }

    @Test
    fun `WHEN entering pdf viewer THEN mark the current tab as showing a pdf`() = runTest {
        val currentTabId = "test"
        val currentTab = TabSessionState(
            id = currentTabId,
            content = ContentState(
                url = "https://mozilla.org",
                isPdf = false,
            ),
        )
        val browserStore = BrowserStore(
            initialState = BrowserState(tabs = listOf(mock(), currentTab)),
        )

        browserStore.dispatch(ContentAction.EnteredPdfViewer(currentTabId)).join()

        assertTrue(browserStore.state.findTabOrCustomTabOrSelectedTab(currentTabId)!!.content.isPdf)
    }

    @Test
    fun `WHEN exiting pdf viewer THEN mark the current tab as not showing a pdf`() = runTest {
        val currentTabId = "test"
        val currentTab = TabSessionState(
            id = currentTabId,
            content = ContentState(
                url = "https://mozilla.org",
                isPdf = true,
            ),
        )
        val browserStore = BrowserStore(
            initialState = BrowserState(tabs = listOf(mock(), currentTab)),
        )

        browserStore.dispatch(ContentAction.ExitedPdfViewer(currentTabId)).join()

        assertFalse(browserStore.state.findTabOrCustomTabOrSelectedTab(currentTabId)!!.content.isPdf)
    }
}
