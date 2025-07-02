/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import androidx.lifecycle.Lifecycle.State.RESUMED
import io.mockk.mockk
import mozilla.components.browser.state.action.SearchAction.ApplicationSearchEnginesLoaded
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SearchState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.lifecycle.TestLifecycleOwner
import org.mozilla.fenix.search.SearchFragmentAction.EnvironmentCleared
import org.mozilla.fenix.search.SearchFragmentAction.EnvironmentRehydrated
import org.mozilla.fenix.search.fixtures.EMPTY_SEARCH_FRAGMENT_STATE
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class BrowserStoreToFenixSearchMapperMiddlewareTest {
    @Test
    fun `WHEN the browser search state changes THEN update the application search state`() {
        val defaultSearchEngine: SearchEngine = mockk()
        val newSearchEngines: List<SearchEngine> = listOf(defaultSearchEngine, mockk())
        val browserStore = BrowserStore(
            BrowserState(
                search = SearchState(
                    applicationSearchEngines = newSearchEngines,
                ),
            ),
        )
        val middleware = BrowserStoreToFenixSearchMapperMiddleware(browserStore)
        val searchStore = buildStore(middleware)

        browserStore.dispatch(ApplicationSearchEnginesLoaded(newSearchEngines))

        assertEquals(defaultSearchEngine, searchStore.state.defaultEngine)
    }

    @Test
    fun `GIVEN an environment was already set WHEN it is cleared THEN reset it to null`() {
        val middleware = BrowserStoreToFenixSearchMapperMiddleware(mockk(relaxed = true))
        val store = buildStore(middleware)

        assertNotNull(middleware.environment)

        store.dispatch(EnvironmentCleared)

        assertNull(middleware.environment)
    }

    private fun buildStore(middleware: BrowserStoreToFenixSearchMapperMiddleware) = SearchFragmentStore(
        initialState = EMPTY_SEARCH_FRAGMENT_STATE,
        middleware = listOf(middleware),
    ).also {
        it.dispatch(
            EnvironmentRehydrated(
                SearchFragmentStore.Environment(
                    context = testContext,
                    viewLifecycleOwner = TestLifecycleOwner(RESUMED),
                    browsingModeManager = mockk(),
                    navController = mockk(),
                ),
            ),
        )
    }
}
