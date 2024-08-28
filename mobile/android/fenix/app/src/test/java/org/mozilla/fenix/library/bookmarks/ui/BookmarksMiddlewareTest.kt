/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

import androidx.navigation.NavBackStackEntry
import androidx.navigation.NavController
import androidx.navigation.NavDestination
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.concept.storage.BookmarksStorage
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.library.bookmarks.friendlyRootTitle

@RunWith(AndroidJUnit4::class)
class BookmarksMiddlewareTest {

    @get:Rule
    val coroutineRule = MainCoroutineRule()

    private val bookmarksStorage: BookmarksStorage = mock()
    private val navController: NavController = mock()
    private lateinit var getBrowsingMode: () -> BrowsingMode
    private lateinit var openTab: (String, Boolean) -> Unit
    private val resolveFolderTitle = { node: BookmarkNode ->
        friendlyRootTitle(
            mock(),
            node,
            true,
            rootTitles = mapOf(
                "root" to "Bookmarks",
                "mobile" to "Bookmarks",
                "menu" to "Bookmarks Menu",
                "toolbar" to "Bookmarks Toolbar",
                "unfiled" to "Other Bookmarks",
            ),
        ) ?: "Bookmarks"
    }

    @Before
    fun setup() {
        getBrowsingMode = { BrowsingMode.Normal }
        openTab = { _, _ -> }
    }

    @Test
    fun `GIVEN bookmarks in storage WHEN store is initialized THEN bookmarks will be loaded as display format`() = runTestOnMain {
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(generateBookmarkTree())
        val middleware = buildMiddleware()

        val store = middleware.makeStore()

        assertEquals(10, store.state.bookmarkItems.size)
    }

    @Test
    fun `GIVEN no bookmarks under mobile root WHEN store is initialized THEN list of bookmarks will be empty`() = runTestOnMain {
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(null)
        val middleware = buildMiddleware()

        val store = middleware.makeStore()

        assertEquals(0, store.state.bookmarkItems.size)
    }

    @Test
    fun `GIVEN last destination was home fragment and in normal browsing mode WHEN a bookmark is clicked THEN open it as a new tab`() {
        val url = "url"
        val bookmarkItem = BookmarkItem.Bookmark(url, "title", url)
        navController.mockBackstack(R.id.homeFragment)
        getBrowsingMode = { BrowsingMode.Normal }
        var capturedUrl = ""
        var capturedNewTab = false
        openTab = { urlCalled, newTab ->
            capturedUrl = urlCalled
            capturedNewTab = newTab
        }

        val middleware = buildMiddleware()
        val store = middleware.makeStore(
            initialState = BookmarksState.default.copy(
                bookmarkItems = listOf(bookmarkItem),
            ),
        )

        store.dispatch(BookmarkClicked(bookmarkItem))

        assertEquals(url, capturedUrl)
        assertTrue(capturedNewTab)
    }

    @Test
    fun `GIVEN last destination was browser fragment and in normal browsing mode WHEN a bookmark is clicked THEN open it in current tab`() {
        val url = "url"
        val bookmarkItem = BookmarkItem.Bookmark(url, "title", url)
        navController.mockBackstack(R.id.browserFragment)
        getBrowsingMode = { BrowsingMode.Normal }
        var capturedUrl = ""
        var capturedNewTab = true
        openTab = { urlCalled, newTab ->
            capturedUrl = urlCalled
            capturedNewTab = newTab
        }

        val middleware = buildMiddleware()
        val store = middleware.makeStore(
            initialState = BookmarksState.default.copy(
                bookmarkItems = listOf(bookmarkItem),
            ),
        )

        store.dispatch(BookmarkClicked(bookmarkItem))

        assertEquals(url, capturedUrl)
        assertFalse(capturedNewTab)
    }

    @Test
    fun `GIVEN in private browsing mode and last destination was home fragment WHEN a bookmark is clicked THEN open it in new tab`() {
        val url = "url"
        val bookmarkItem = BookmarkItem.Bookmark(url, "title", url)
        navController.mockBackstack(R.id.homeFragment)
        getBrowsingMode = { BrowsingMode.Private }
        var capturedUrl = ""
        var capturedNewTab = false
        openTab = { urlCalled, newTab ->
            capturedUrl = urlCalled
            capturedNewTab = newTab
        }

        val middleware = buildMiddleware()
        val store = middleware.makeStore(
            initialState = BookmarksState.default.copy(
                bookmarkItems = listOf(bookmarkItem),
            ),
        )

        store.dispatch(BookmarkClicked(bookmarkItem))

        assertEquals(url, capturedUrl)
        assertTrue(capturedNewTab)
    }

    @Test
    fun `GIVEN in private browsing mode and last destination was browser fragment WHEN a bookmark is clicked THEN open it in new tab`() {
        val url = "url"
        val bookmarkItem = BookmarkItem.Bookmark(url, "title", url)
        navController.mockBackstack(R.id.browserFragment)
        getBrowsingMode = { BrowsingMode.Private }
        var capturedUrl = ""
        var capturedNewTab = false
        openTab = { urlCalled, newTab ->
            capturedUrl = urlCalled
            capturedNewTab = newTab
        }

        val middleware = buildMiddleware()
        val store = middleware.makeStore(
            initialState = BookmarksState.default.copy(
                bookmarkItems = listOf(bookmarkItem),
            ),
        )

        store.dispatch(BookmarkClicked(bookmarkItem))

        assertEquals(url, capturedUrl)
        assertTrue(capturedNewTab)
    }

    @Test
    fun `WHEN folder is clicked THEN children are loaded and screen title is updated to folder title`() = runTestOnMain {
        val folderNode = bookmarkTree.first { it.type == BookmarkNodeType.FOLDER }
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(generateBookmarkTree())
        `when`(bookmarksStorage.getTree(folderNode.guid)).thenReturn(generateBookmarkFolder(folderNode.guid, folderNode.title!!))

        val middleware = buildMiddleware()
        val store = middleware.makeStore(
            initialState = BookmarksState.default,
        )
        store.dispatch(FolderClicked(BookmarkItem.Folder(folderNode.title!!, folderNode.guid)))

        assertEquals(folderNode.title, store.state.folderTitle)
        assertEquals(5, store.state.bookmarkItems.size)
    }

    @Test
    fun `WHEN search button is clicked THEN navigate to search`() {
        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        store.dispatch(SearchClicked)

        verify(navController).navigate(NavGraphDirections.actionGlobalSearchDialog(sessionId = null))
    }

    private fun buildMiddleware() = BookmarksMiddleware(
        bookmarksStorage = bookmarksStorage,
        navController = navController,
        resolveFolderTitle = resolveFolderTitle,
        getBrowsingMode = getBrowsingMode,
        openTab = openTab,
        scope = coroutineRule.scope,
    )

    private fun BookmarksMiddleware.makeStore(
        initialState: BookmarksState = BookmarksState.default,
    ) = BookmarksStore(
        initialState = initialState,
        middleware = listOf(this),
    )

    private val bookmarkFolders = List(5) {
        generateBookmarkFolder("folder guid $it", "folder title $it")
    }

    private val bookmarkItems = List(5) {
        generateBookmark("item guid $it", "item title $it", "item url $it")
    }

    private val bookmarkTree = bookmarkFolders + bookmarkItems

    private fun generateBookmarkTree() = BookmarkNode(
        type = BookmarkNodeType.FOLDER,
        guid = BookmarkRoot.Mobile.id,
        parentGuid = null,
        position = 0U,
        title = "mobile",
        url = null,
        dateAdded = 0L,
        children = bookmarkFolders + bookmarkItems,
    )

    private fun generateBookmarkFolder(guid: String, title: String) = BookmarkNode(
        type = BookmarkNodeType.FOLDER,
        guid = guid,
        parentGuid = null,
        position = 0U,
        title = title,
        url = null,
        dateAdded = 0L,
        children = bookmarkItems,
    )

    private fun generateBookmark(guid: String, title: String, url: String) = BookmarkNode(
        type = BookmarkNodeType.ITEM,
        guid = guid,
        parentGuid = null,
        position = 0U,
        title = title,
        url = url,
        dateAdded = 0L,
        children = listOf(),
    )

    private fun NavController.mockBackstack(expectedId: Int) {
        val destination = mock<NavDestination>()
        val backStackEntry = mock<NavBackStackEntry>()
        `when`(destination.id).thenReturn(expectedId)
        `when`(backStackEntry.destination).thenReturn(destination)
        `when`(previousBackStackEntry).thenReturn(backStackEntry)
    }
}
