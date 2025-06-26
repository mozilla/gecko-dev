/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry
import io.mockk.every
import io.mockk.mockk
import mozilla.components.compose.browser.toolbar.store.BrowserEditToolbarAction.UpdateEditText
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarAction.ToggleEditMode
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarStore
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.search.BrowserToolbarToFenixSearchMapperMiddleware.LifecycleDependencies
import org.mozilla.fenix.search.SearchFragmentAction.SearchStarted
import org.mozilla.fenix.search.fixtures.EMPTY_SEARCH_FRAGMENT_STATE
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class BrowserToolbarToFenixSearchMapperMiddlewareTest {
    val toolbarStore = BrowserToolbarStore()
    private val browsingModeManager: BrowsingModeManager = mockk {
        every { mode } returns BrowsingMode.Private
    }

    @Test
    fun `WHEN edit mode is entered THEN set search state as started`() {
        val searchStatusMapperMiddleware = buildMiddleware()
        val captorMiddleware = CaptureActionsMiddleware<SearchFragmentState, SearchFragmentAction>()
        val searchStore = SearchFragmentStore(
            initialState = emptySearchState,
            middleware = listOf(searchStatusMapperMiddleware, captorMiddleware),
        )

        toolbarStore.dispatch(ToggleEditMode(true))

        captorMiddleware.assertLastAction(SearchStarted::class) {
            assertNull(it.selectedSearchEngine)
            assertTrue(it.inPrivateMode)
        }
    }

    @Test
    fun `GIVEN search was started WHEN there's a new query in the toolbar THEN update the search state`() {
        val (_, searchStore) = buildMiddlewareAndAddToSearchStore()
        toolbarStore.dispatch(ToggleEditMode(true))

        searchStore.dispatch(SearchStarted(mockk(), false))

        toolbarStore.dispatch(UpdateEditText("t"))
        assertEquals("t", searchStore.state.query)

        toolbarStore.dispatch(UpdateEditText("te"))
        assertEquals("te", searchStore.state.query)

        toolbarStore.dispatch(UpdateEditText("tes"))
        assertEquals("tes", searchStore.state.query)

        toolbarStore.dispatch(UpdateEditText("test"))
        assertEquals("test", searchStore.state.query)
    }

    private fun buildMiddlewareAndAddToSearchStore(
        toolbarStore: BrowserToolbarStore = this.toolbarStore,
        browsingModeManager: BrowsingModeManager = this.browsingModeManager,
        lifecycleOwner: LifecycleOwner = MockedLifecycleOwner(Lifecycle.State.RESUMED),
    ): Pair<BrowserToolbarToFenixSearchMapperMiddleware, SearchFragmentStore> {
        val middleware = buildMiddleware(toolbarStore, browsingModeManager, lifecycleOwner)
        val searchStore = SearchFragmentStore(
            initialState = emptySearchState,
            middleware = listOf(middleware),
        )
        return middleware to searchStore
    }

    private fun buildMiddleware(
        toolbarStore: BrowserToolbarStore = this.toolbarStore,
        browsingModeManager: BrowsingModeManager = this.browsingModeManager,
        lifecycleOwner: LifecycleOwner = MockedLifecycleOwner(Lifecycle.State.RESUMED),
    ) = BrowserToolbarToFenixSearchMapperMiddleware(toolbarStore).apply {
        updateLifecycleDependencies(LifecycleDependencies(browsingModeManager, lifecycleOwner))
    }

    private class MockedLifecycleOwner(initialState: Lifecycle.State) : LifecycleOwner {
        override val lifecycle: Lifecycle = LifecycleRegistry(this).apply {
            currentState = initialState
        }
    }

    private val emptySearchState = EMPTY_SEARCH_FRAGMENT_STATE.copy(
        searchEngineSource = mockk(),
        defaultEngine = mockk(),
        showSearchTermHistory = true,
        showQrButton = true,
    )
}
