/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.search

import io.mockk.every
import io.mockk.mockk
import io.mockk.slot
import io.mockk.verify
import kotlinx.coroutines.test.advanceUntilIdle
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.InitAction
import mozilla.components.browser.state.action.SearchAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Store
import org.junit.Assert.assertEquals
import org.junit.Test

class ApplicationSearchMiddlewareTest {
    @Test
    fun `GIVEN ApplicationSearchLoaderMiddleware WHEN InitAction is received THEN dispatch ApplicationSearchEnginesLoaded`() = runTest {
        val middleware = ApplicationSearchMiddleware(
            mockk(),
            { _ -> "mockk()" },
            { _ -> mockk() },
            this,
        )
        val store: Store<BrowserState, BrowserAction> = mockk(relaxed = true)
        val context: MiddlewareContext<BrowserState, BrowserAction> = mockk()
        every { context.store } returns store

        middleware.invoke(context, { _ -> }, InitAction)
        this.advanceUntilIdle()

        val slot = slot<SearchAction.ApplicationSearchEnginesLoaded>()
        verify { store.dispatch(capture(slot)) }
        slot.captured.applicationSearchEngines.also {
            assertEquals(3, it.size)
            assertEquals("bookmarks_search_engine_id", it.first().id)
        }
    }
}
