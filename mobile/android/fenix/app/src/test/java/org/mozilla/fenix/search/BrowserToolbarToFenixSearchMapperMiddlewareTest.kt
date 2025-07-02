/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import androidx.lifecycle.Lifecycle
import io.mockk.every
import io.mockk.mockk
import mozilla.components.compose.browser.toolbar.store.BrowserEditToolbarAction.SearchQueryUpdated
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarAction.ToggleEditMode
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarStore
import mozilla.components.lib.state.Middleware
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.helpers.lifecycle.TestLifecycleOwner
import org.mozilla.fenix.search.SearchFragmentAction.EnvironmentCleared
import org.mozilla.fenix.search.SearchFragmentAction.EnvironmentRehydrated
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
    fun `WHEN entering in edit mode THEN consider it as search being started`() {
        val searchStatusMapperMiddleware = buildMiddleware()
        val captorMiddleware = CaptureActionsMiddleware<SearchFragmentState, SearchFragmentAction>()
        val searchStore = buildSearchStore(listOf(searchStatusMapperMiddleware, captorMiddleware))

        toolbarStore.dispatch(ToggleEditMode(true))

        captorMiddleware.assertLastAction(SearchStarted::class) {
            assertNull(it.selectedSearchEngine)
            assertTrue(it.inPrivateMode)
        }
    }

    @Test
    fun `GIVEN an environment was already set WHEN it is cleared THEN reset it to null`() {
        val searchStatusMapperMiddleware = buildMiddleware()
        val searchStore = buildSearchStore(listOf(searchStatusMapperMiddleware))

        assertNotNull(searchStatusMapperMiddleware.environment)

        searchStore.dispatch(EnvironmentCleared)

        assertNull(searchStatusMapperMiddleware.environment)
    }

    @Test
    fun `GIVEN search was started WHEN there's a new query in the toolbar THEN update the search state`() {
        val searchStore = buildSearchStore(listOf(buildMiddleware()))
        toolbarStore.dispatch(ToggleEditMode(true))

        searchStore.dispatch(SearchStarted(mockk(), false, false))

        toolbarStore.dispatch(SearchQueryUpdated("t"))
        assertEquals("t", searchStore.state.query)

        toolbarStore.dispatch(SearchQueryUpdated("te"))
        assertEquals("te", searchStore.state.query)

        toolbarStore.dispatch(SearchQueryUpdated("tes"))
        assertEquals("tes", searchStore.state.query)

        toolbarStore.dispatch(SearchQueryUpdated("test"))
        assertEquals("test", searchStore.state.query)
    }

    private fun buildSearchStore(
        middlewares: List<Middleware<SearchFragmentState, SearchFragmentAction>> = emptyList(),
    ) = SearchFragmentStore(
        initialState = emptySearchState,
        middleware = middlewares,
    ).also {
        it.dispatch(
            EnvironmentRehydrated(
                SearchFragmentStore.Environment(
                    context = testContext,
                    viewLifecycleOwner = TestLifecycleOwner(Lifecycle.State.RESUMED),
                    browsingModeManager = browsingModeManager,
                    navController = mockk(),
                ),
            ),
        )
    }

    private fun buildMiddleware(
        toolbarStore: BrowserToolbarStore = this.toolbarStore,
    ) = BrowserToolbarToFenixSearchMapperMiddleware(toolbarStore)

    private val emptySearchState = EMPTY_SEARCH_FRAGMENT_STATE.copy(
        searchEngineSource = mockk(),
        defaultEngine = mockk(),
        showSearchTermHistory = true,
        showQrButton = true,
    )
}
