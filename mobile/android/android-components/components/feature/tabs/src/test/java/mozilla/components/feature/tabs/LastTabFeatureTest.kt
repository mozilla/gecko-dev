/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

class LastTabFeatureTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `onBackPressed removes the session if it was opened by an ACTION_VIEW intent`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(
                        url = "https://www.mozilla.org",
                        id = "A",
                        source = SessionState.Source.External.ActionView(mock()),
                    ),
                ),
                selectedTabId = "A",
            ),
        )

        val useCase: TabsUseCases.RemoveTabUseCase = mock()

        val feature = LastTabFeature(store, "A", mock(), mock())

        assertTrue(feature.onBackPressed())
        verify(useCase, never()).invoke("A")
    }

    @Test
    fun `onBackPressed() removes the session if it has a parent session and no more history`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "A"),
                    createTab("https://www.mozilla.org", id = "B", parentId = "A"),
                ),
                selectedTabId = "A",
            ),
        )

        val useCase: TabsUseCases.RemoveTabUseCase = mock()

        val feature = LastTabFeature(store, "B", mock(), mock())

        assertTrue(feature.onBackPressed())
        verify(useCase, never()).invoke("A")
    }
}
