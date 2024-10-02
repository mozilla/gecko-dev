/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.file

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class FileUploadsDirCleanerMiddlewareTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val dispatcher = coroutinesTestRule.testDispatcher

    @Test
    fun `WHEN an action that indicates the user has navigated to another website THEN clean up temporary uploads`() {
        val fileUploadsDirCleaner = mock<FileUploadsDirCleaner>()
        val tab = createTab("https://www.mozilla.org", id = "test-tab")
        val store = BrowserStore(
            middleware = listOf(
                FileUploadsDirCleanerMiddleware(
                    fileUploadsDirCleaner = fileUploadsDirCleaner,
                ),
            ),
            initialState = BrowserState(
                tabs = listOf(tab),
            ),
        )

        store.dispatch(ContentAction.UpdateUrlAction("test-tab", "https://www.wikipedia.org")).joinBlocking()
        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        verify(fileUploadsDirCleaner).cleanRecentUploads()

        store.dispatch(ContentAction.UpdateUrlAction("test-tab", "https://www.wikipedia.org/cats")).joinBlocking()
        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        // Same site, no cleanups expected
        verify(fileUploadsDirCleaner, times(1)).cleanRecentUploads()

        store.dispatch(ContentAction.UpdateUrlAction("test-tab", "https://www.example.com")).joinBlocking()
        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        // Navigating to another  site clean up expected
        verify(fileUploadsDirCleaner, times(2)).cleanRecentUploads()
    }

    @Test
    fun `GIVEN a subdomain WHEN an action that indicates the user has navigated to another website THEN clean up temporary uploads`() {
        val fileUploadsDirCleaner = mock<FileUploadsDirCleaner>()
        val tab = createTab("https://mozilla.org", id = "test-tab")
        val store = BrowserStore(
            middleware = listOf(
                FileUploadsDirCleanerMiddleware(
                    fileUploadsDirCleaner = fileUploadsDirCleaner,
                ),
            ),
            initialState = BrowserState(
                tabs = listOf(tab),
            ),
        )

        store.dispatch(ContentAction.UpdateUrlAction("test-tab", "https://www.mozilla.org")).joinBlocking()
        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        verify(fileUploadsDirCleaner).cleanRecentUploads()
    }

    @Test
    fun `GIVEN there are not temporary uploads WHEN an action that indicates the user has navigated to another website THEN do not try to clean up temporary uploads `() {
        val fileUploadsDirCleaner = mock<FileUploadsDirCleaner>()
        val tab = createTab("https://www.mozilla.org", id = "test-tab")
        val store = BrowserStore(
            middleware = listOf(
                FileUploadsDirCleanerMiddleware(
                    fileUploadsDirCleaner = fileUploadsDirCleaner,
                ),
            ),
            initialState = BrowserState(
                tabs = listOf(tab),
            ),
        )

        store.dispatch(ContentAction.UpdateUrlAction("test-tab", "https://www.wikipedia.org")).joinBlocking()
        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        verify(fileUploadsDirCleaner, times(0)).performCleanRecentUploads()
    }
}
