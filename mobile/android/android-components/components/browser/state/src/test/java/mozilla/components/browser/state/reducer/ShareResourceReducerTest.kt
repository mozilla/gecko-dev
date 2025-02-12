/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.ShareResourceAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.content.ShareResourceState
import mozilla.components.concept.fetch.Response
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class ShareResourceReducerTest {

    @Test
    fun `reduce - AddShareAction should add the internetResource in the ContentState`() {
        val reducer = ShareResourceStateReducer
        val state = BrowserState(tabs = listOf(TabSessionState("tabId", ContentState("contentStateUrl"))))
        val response: Response = mock()
        val action = ShareResourceAction.AddShareAction(
            "tabId",
            ShareResourceState.InternetResource("internetResourceUrl", "type", true, response),
        )

        assertNull(state.tabs[0].content.share)

        val result = reducer.reduce(state, action)

        val shareState = result.tabs[0].content.share!! as ShareResourceState.InternetResource
        assertEquals("internetResourceUrl", shareState.url)
        assertEquals("type", shareState.contentType)
        assertTrue(shareState.private)
        assertEquals(response, shareState.response)
    }

    @Test
    fun `reduce - AddShareAction should add the localResource in the ContentState`() {
        val reducer = ShareResourceStateReducer
        val state = BrowserState(tabs = listOf(TabSessionState("tabId", ContentState("contentStateUrl"))))
        val action = ShareResourceAction.AddShareAction(
            "tabId",
            ShareResourceState.LocalResource("internetResourceUrl", "type"),
        )

        assertNull(state.tabs[0].content.share)

        val result = reducer.reduce(state, action)

        val shareState = result.tabs[0].content.share!! as ShareResourceState.LocalResource
        assertEquals("internetResourceUrl", shareState.url)
        assertEquals("type", shareState.contentType)
    }

    @Test
    fun `reduce - ConsumeShareAction should remove the ContentState's share resource`() {
        val reducer = ShareResourceStateReducer
        val shareState: ShareResourceState.InternetResource = mock()
        val state = BrowserState(
            tabs = listOf(
                TabSessionState("tabId", ContentState("contentStateUrl", share = shareState)),
            ),
        )
        val action = ShareResourceAction.ConsumeShareAction("tabId")

        assertNotNull(state.tabs[0].content.share)

        val result = reducer.reduce(state, action)

        assertNull(result.tabs[0].content.share)
    }
}
