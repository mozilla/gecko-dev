/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history.state

import kotlinx.coroutines.test.advanceUntilIdle
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.RecentlyClosedAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mockito.Mockito.anyLong
import org.mockito.Mockito.verify
import org.mozilla.fenix.library.history.HistoryFragmentAction
import org.mozilla.fenix.library.history.HistoryFragmentState
import org.mozilla.fenix.library.history.HistoryFragmentStore
import org.mozilla.fenix.library.history.RemoveTimeFrame

class HistoryStorageMiddlewareTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private lateinit var captureBrowserActions: CaptureActionsMiddleware<BrowserState, BrowserAction>
    private lateinit var browserStore: BrowserStore
    private lateinit var storage: PlacesHistoryStorage

    @Before
    fun setup() {
        storage = mock()
        captureBrowserActions = CaptureActionsMiddleware()
        browserStore = BrowserStore(middleware = listOf(captureBrowserActions))
    }

    @Test
    fun `WHEN a null time frame is deleted THEN browser store is informed, storage deletes everything, and callback is invoked`() = runTestOnMain {
        var callbackInvoked = false
        val middleware = HistoryStorageMiddleware(
            browserStore = browserStore,
            historyStorage = storage,
            onTimeFrameDeleted = { callbackInvoked = true },
            scope = this,
        )
        val store = HistoryFragmentStore(
            initialState = HistoryFragmentState.initial,
            middleware = listOf(middleware),
        )

        store.dispatch(HistoryFragmentAction.DeleteTimeRange(null)).joinBlocking()
        store.waitUntilIdle()
        browserStore.waitUntilIdle()

        assertTrue(callbackInvoked)
        assertEquals(HistoryFragmentState.Mode.Normal, store.state.mode)
        captureBrowserActions.assertFirstAction(RecentlyClosedAction.RemoveAllClosedTabAction::class)
        captureBrowserActions.assertLastAction(EngineAction.PurgeHistoryAction::class) {}
        verify(storage).deleteEverything()
    }

    @Ignore("Intermittent failure; see Bug 1848436.")
    @Test
    fun `WHEN a specified time frame is deleted THEN browser store is informed, storage deletes time frame, and callback is invoked`() = runTestOnMain {
        var callbackInvoked = false
        val middleware = HistoryStorageMiddleware(
            browserStore = browserStore,
            historyStorage = storage,
            onTimeFrameDeleted = { callbackInvoked = true },
            scope = this,
        )
        val store = HistoryFragmentStore(
            initialState = HistoryFragmentState.initial,
            middleware = listOf(middleware),
        )

        store.dispatch(HistoryFragmentAction.DeleteTimeRange(RemoveTimeFrame.LastHour)).joinBlocking()
        store.waitUntilIdle()
        browserStore.waitUntilIdle()
        this.advanceUntilIdle()

        assertTrue(callbackInvoked)
        assertFalse(store.state.isDeletingItems)
        captureBrowserActions.assertFirstAction(RecentlyClosedAction.RemoveAllClosedTabAction::class)
        captureBrowserActions.assertLastAction(EngineAction.PurgeHistoryAction::class) {}
        verify(storage).deleteVisitsBetween(anyLong(), anyLong())
    }
}
