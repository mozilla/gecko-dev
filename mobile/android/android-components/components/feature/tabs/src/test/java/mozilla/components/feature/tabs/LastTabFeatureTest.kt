/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

class LastTabFeatureTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `onBackPressed() removes the session if it was opened by an ACTION_VIEW intent`() {
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
        val feature = LastTabFeature(store, "A", useCase, mock())

        assertTrue(feature.onBackPressed())
        verify(useCase).invoke("A")
    }

    @Test
    fun `onBackPressed() does not remove the last tab if there is no parent`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(
                        url = "https://www.mozilla.org",
                        id = "A",
                    ),
                ),
                selectedTabId = "A",
            ),
        )

        val useCase: TabsUseCases.RemoveTabUseCase = mock()
        val feature = LastTabFeature(store, "A", useCase, mock())

        assertFalse(feature.onBackPressed())
        verify(useCase, never()).invoke("A", true)
    }

    @Test
    fun `onBackPressed() removes the tabId session if it has a parent session`() {
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
        val feature = LastTabFeature(store, "B", useCase, mock())

        assertTrue(feature.onBackPressed())
        verify(useCase).invoke("B", true)
    }

    @Test
    fun `onBackPressed() does not remove the selected session if it doesn't have a parent`() {
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
        val feature = LastTabFeature(
            store = store,
            removeTabUseCase = useCase,
            activity = mock(),
        )

        assertFalse(feature.onBackPressed())
        verify(useCase, never()).invoke(any(), anyBoolean())
    }
}
