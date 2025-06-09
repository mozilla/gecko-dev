/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppState

@RunWith(AndroidJUnit4::class)
class PrivateBrowsingLockMiddlewareTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    // Locked cases

    @Test
    fun `GIVEN private mode is locked WHEN a bookmark was opened in private mode THEN require verification and don't let action pass further and cache it`() {
        val isPrivateScreenLocked = true
        var verificationRequested = false
        var testMiddlewareInvoked = false
        val action = BookmarksListMenuAction.Bookmark.OpenInPrivateTabClicked(generateBookmark())
        val middleware = PrivateBrowsingLockMiddleware(
            AppStore(initialState = AppState(isPrivateScreenLocked = isPrivateScreenLocked)),
        ) {
            verificationRequested = true
        }
        val store = middleware.makeStore(
            testMiddlewareExpectedAction = action,
            onTestMiddlewareInvoked = {
                testMiddlewareInvoked = true
            },
        )

        assertFalse(verificationRequested)
        assertFalse(testMiddlewareInvoked)
        assertNull(middleware.pendingAction)

        store.dispatch(action).joinBlocking()
        store.waitUntilIdle()

        assertTrue(verificationRequested)
        assertFalse(testMiddlewareInvoked)
        assertNotNull(middleware.pendingAction)
    }

    @Test
    fun `GIVEN private mode is locked WHEN multiple bookmarks are opened in private mode THEN require verification and don't let action pass further and cache it`() {
        val isPrivateScreenLocked = true
        var verificationRequested = false
        var testMiddlewareInvoked = false
        val action = BookmarksListMenuAction.MultiSelect.OpenInPrivateTabsClicked
        val middleware = PrivateBrowsingLockMiddleware(
            AppStore(initialState = AppState(isPrivateScreenLocked = isPrivateScreenLocked)),
        ) {
            verificationRequested = true
        }
        val store = middleware.makeStore(
            testMiddlewareExpectedAction = action,
            onTestMiddlewareInvoked = {
                testMiddlewareInvoked = true
            },
        )

        assertFalse(verificationRequested)
        assertFalse(testMiddlewareInvoked)
        assertNull(middleware.pendingAction)

        store.dispatch(action).joinBlocking()
        store.waitUntilIdle()

        assertTrue(verificationRequested)
        assertFalse(testMiddlewareInvoked)
        assertNotNull(middleware.pendingAction)
    }

    @Test
    fun `GIVEN private mode is locked WHEN all tabs in a folder are opened in private mode THEN require verification and don't let action pass further and cache it`() {
        val isPrivateScreenLocked = true
        var verificationRequested = false
        var testMiddlewareInvoked = false
        val action = BookmarksListMenuAction.Folder.OpenAllInPrivateTabClicked(generateFolder())
        val middleware = PrivateBrowsingLockMiddleware(
            AppStore(initialState = AppState(isPrivateScreenLocked = isPrivateScreenLocked)),
        ) {
            verificationRequested = true
        }
        val store = middleware.makeStore(
            testMiddlewareExpectedAction = action,
            onTestMiddlewareInvoked = {
                testMiddlewareInvoked = true
            },
        )

        assertFalse(verificationRequested)
        assertFalse(testMiddlewareInvoked)
        assertNull(middleware.pendingAction)

        store.dispatch(action).joinBlocking()
        store.waitUntilIdle()

        assertTrue(verificationRequested)
        assertFalse(testMiddlewareInvoked)
        assertNotNull(middleware.pendingAction)
    }

    // Unlocked cases

    @Test
    fun `GIVEN private mode is unlocked WHEN a bookmark was opened in private mode THEN do not require verification and let the action through and don't cache it`() {
        val isPrivateScreenLocked = false
        var verificationRequested = false
        var testMiddlewareInvoked = false
        val action = BookmarksListMenuAction.Bookmark.OpenInPrivateTabClicked(generateBookmark())
        val middleware = PrivateBrowsingLockMiddleware(
            AppStore(initialState = AppState(isPrivateScreenLocked = isPrivateScreenLocked)),
        ) {
            verificationRequested = true
        }
        val store = middleware.makeStore(
            testMiddlewareExpectedAction = action,
            onTestMiddlewareInvoked = {
                testMiddlewareInvoked = true
            },
        )

        assertFalse(verificationRequested)
        assertFalse(testMiddlewareInvoked)
        assertNull(middleware.pendingAction)

        store.dispatch(action).joinBlocking()
        store.waitUntilIdle()

        assertFalse(verificationRequested)
        assertTrue(testMiddlewareInvoked)
        assertNull(middleware.pendingAction)
    }

    @Test
    fun `GIVEN private mode is unlocked WHEN multiple bookmarks are opened in private mode THEN do not require verification and let the action through and don't cache it`() {
        val isPrivateScreenLocked = false
        var verificationRequested = false
        var testMiddlewareInvoked = false
        val action = BookmarksListMenuAction.MultiSelect.OpenInPrivateTabsClicked
        val middleware = PrivateBrowsingLockMiddleware(
            AppStore(initialState = AppState(isPrivateScreenLocked = isPrivateScreenLocked)),
        ) {
            verificationRequested = true
        }
        val store = middleware.makeStore(
            testMiddlewareExpectedAction = action,
            onTestMiddlewareInvoked = {
                testMiddlewareInvoked = true
            },
        )

        assertFalse(verificationRequested)
        assertFalse(testMiddlewareInvoked)
        assertNull(middleware.pendingAction)

        store.dispatch(action).joinBlocking()
        store.waitUntilIdle()

        assertFalse(verificationRequested)
        assertTrue(testMiddlewareInvoked)
        assertNull(middleware.pendingAction)
    }

    @Test
    fun `GIVEN private mode is unlocked WHEN all tabs in a folder are opened in private mode THEN do not require verification and let the action through and don't cache it`() {
        val isPrivateScreenLocked = false
        var verificationRequested = false
        var testMiddlewareInvoked = false
        val action = BookmarksListMenuAction.Folder.OpenAllInPrivateTabClicked(generateFolder())
        val middleware = PrivateBrowsingLockMiddleware(
            AppStore(initialState = AppState(isPrivateScreenLocked = isPrivateScreenLocked)),
        ) {
            verificationRequested = true
        }
        val store = middleware.makeStore(
            testMiddlewareExpectedAction = action,
            onTestMiddlewareInvoked = {
                testMiddlewareInvoked = true
            },
        )

        assertFalse(verificationRequested)
        assertFalse(testMiddlewareInvoked)
        assertNull(middleware.pendingAction)

        store.dispatch(action).joinBlocking()
        store.waitUntilIdle()

        assertFalse(verificationRequested)
        assertTrue(testMiddlewareInvoked)
        assertNull(middleware.pendingAction)
    }

    // Release action

    @Test
    fun `WHEN action PrivateBrowsingAuthorized THEN evoke cached action and null it`() {
        var testMiddlewareInvoked = false
        val action = PrivateBrowsingAuthorized
        val middleware = PrivateBrowsingLockMiddleware(AppStore()) {}.apply {
            pendingAction = BookmarksListMenuAction.MultiSelect.OpenInPrivateTabsClicked
        }

        val store = middleware.makeStore(
            testMiddlewareExpectedAction = action,
            onTestMiddlewareInvoked = {
                testMiddlewareInvoked = true
            },
        )

        assertFalse(testMiddlewareInvoked)
        assertNotNull(middleware.pendingAction)

        store.dispatch(action).joinBlocking()
        store.waitUntilIdle()

        assertTrue(testMiddlewareInvoked)
        assertNull(middleware.pendingAction)
    }

    private fun PrivateBrowsingLockMiddleware.makeStore(
        testMiddlewareExpectedAction: BookmarksAction? = null,
        onTestMiddlewareInvoked: () -> Unit = {},
    ) = BookmarksStore(
        initialState = BookmarksState.default,
        middleware = listOf(
            this,
            TestMiddleware(testMiddlewareExpectedAction, onTestMiddlewareInvoked),
        ),
    )

    private fun generateBookmark(
        num: Int = 0,
        url: String = "url",
        title: String = "title",
        previewImageUrl: String = "previewImageUrl",
        dateAdded: Long = 0,
        position: UInt? = null,
    ) = BookmarkItem.Bookmark(url, title, previewImageUrl, "$num", position, dateAdded)

    private fun generateFolder(
        num: Int = 0,
        title: String = "title",
        dateAdded: Long = 0,
        position: UInt? = null,
    ) = BookmarkItem.Folder(title, "$num", position, dateAdded)
}

private class TestMiddleware(
    private val expectedAction: BookmarksAction? = null,
    private val onExpectedActionProcessed: () -> Unit,
) : Middleware<BookmarksState, BookmarksAction> {
    override fun invoke(
        context: MiddlewareContext<BookmarksState, BookmarksAction>,
        next: (BookmarksAction) -> Unit,
        action: BookmarksAction,
    ) {
        if (action == expectedAction) {
            onExpectedActionProcessed()
        }
    }
}
