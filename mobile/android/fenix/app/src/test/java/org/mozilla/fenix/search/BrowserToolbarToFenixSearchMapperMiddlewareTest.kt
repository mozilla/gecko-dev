/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import io.mockk.mockk
import mozilla.components.compose.browser.toolbar.store.BrowserEditToolbarAction.UpdateEditText
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarStore
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.search.BrowserToolbarToFenixSearchMapperMiddleware.LifecycleDependencies
import org.mozilla.fenix.search.SearchFragmentAction.SearchStarted
import org.mozilla.fenix.search.fixtures.EMPTY_SEARCH_FRAGMENT_STATE
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class BrowserToolbarToFenixSearchMapperMiddlewareTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `GIVEN search was started WHEN there's a new query in the toolbar THEN update the search state`() = runTestOnMain {
        val toolbarStore = BrowserToolbarStore()
        val middleware = BrowserToolbarToFenixSearchMapperMiddleware(toolbarStore).apply {
            updateLifecycleDependencies(LifecycleDependencies(coroutinesTestRule.scope))
        }
        val searchStore = SearchFragmentStore(
            initialState = emptySearchState,
            middleware = listOf(middleware),
        )
        searchStore.dispatch(SearchStarted(mockk(), false))
        testScheduler.advanceUntilIdle()

        toolbarStore.dispatch(UpdateEditText("t"))
        assertEquals("t", searchStore.state.query)

        toolbarStore.dispatch(UpdateEditText("te"))
        assertEquals("te", searchStore.state.query)

        toolbarStore.dispatch(UpdateEditText("tes"))
        assertEquals("tes", searchStore.state.query)

        toolbarStore.dispatch(UpdateEditText("test"))
        assertEquals("test", searchStore.state.query)
    }

    private val emptySearchState = EMPTY_SEARCH_FRAGMENT_STATE.copy(
        searchEngineSource = mockk(),
        defaultEngine = mockk(),
        showSearchTermHistory = true,
        showQrButton = true,
    )
}
