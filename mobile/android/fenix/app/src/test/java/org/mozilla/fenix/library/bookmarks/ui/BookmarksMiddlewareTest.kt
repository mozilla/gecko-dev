/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

import androidx.navigation.NavBackStackEntry
import androidx.navigation.NavController
import androidx.navigation.NavDestination
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.test.advanceUntilIdle
import kotlinx.coroutines.test.runTest
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.storage.BookmarkInfo
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.concept.storage.BookmarksStorage
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.times
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

    private lateinit var bookmarksStorage: BookmarksStorage
    private lateinit var navController: NavController
    private lateinit var navigateToSignIntoSync: () -> Unit
    private lateinit var exitBookmarks: () -> Unit
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
        bookmarksStorage = mock()
        navController = mock()
        navigateToSignIntoSync = { }
        exitBookmarks = { }
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
        val bookmarkItem = BookmarkItem.Bookmark(url, "title", url, guid = "")
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
        val bookmarkItem = BookmarkItem.Bookmark(url, "title", url, guid = "")
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
        val bookmarkItem = BookmarkItem.Bookmark(url, "title", url, guid = "")
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
        val bookmarkItem = BookmarkItem.Bookmark(url, "title", url, guid = "")
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
        val bookmarkTree = generateBookmarkTree()
        val folderNode = bookmarkTree.children!!.first { it.type == BookmarkNodeType.FOLDER }
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(generateBookmarkTree())
        `when`(bookmarksStorage.getTree(folderNode.guid))
            .thenReturn(generateBookmarkFolder(folderNode.guid, folderNode.title!!, BookmarkRoot.Mobile.id))

        val middleware = buildMiddleware()
        val store = middleware.makeStore(
            initialState = BookmarksState.default,
        )
        store.dispatch(FolderClicked(BookmarkItem.Folder(folderNode.title!!, folderNode.guid)))

        assertEquals(folderNode.title, store.state.currentFolder.title)
        assertEquals(5, store.state.bookmarkItems.size)
    }

    @Test
    fun `WHEN search button is clicked THEN navigate to search`() {
        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        store.dispatch(SearchClicked)

        verify(navController).navigate(NavGraphDirections.actionGlobalSearchDialog(sessionId = null))
    }

    @Test
    fun `WHEN add folder button is clicked THEN navigate to folder screen`() {
        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        store.dispatch(AddFolderClicked)

        verify(navController).navigate(BookmarksDestinations.ADD_FOLDER)
    }

    @Test
    fun `GIVEN current screen is add folder and new folder title is nonempty WHEN back is clicked THEN navigate back, save the new folder, and load the updated tree`() = runTest {
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(generateBookmarkTree())
        val middleware = buildMiddleware()
        val store = middleware.makeStore()
        val newFolderTitle = "test"

        store.dispatch(AddFolderClicked)
        store.dispatch(AddFolderAction.TitleChanged(newFolderTitle))

        assertNotNull(store.state.bookmarksAddFolderState)

        store.dispatch(BackClicked)

        verify(bookmarksStorage).addFolder(store.state.currentFolder.guid, title = newFolderTitle)
        verify(bookmarksStorage, times(2)).getTree(BookmarkRoot.Mobile.id)
        verify(navController).popBackStack()
        assertNull(store.state.bookmarksAddFolderState)
    }

    @Test
    fun `GIVEN current screen is add folder and new folder title is empty WHEN back is clicked THEN navigate back to the previous tree and don't save anything`() = runTestOnMain {
        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        store.dispatch(AddFolderClicked)
        store.dispatch(AddFolderAction.TitleChanged("test"))
        store.dispatch(AddFolderAction.TitleChanged(""))
        assertNotNull(store.state.bookmarksAddFolderState)

        store.dispatch(BackClicked)
        this.advanceUntilIdle()

        verify(bookmarksStorage, never()).addFolder(parentGuid = store.state.currentFolder.guid, title = "")
        verify(navController).popBackStack()
        assertNull(store.state.bookmarksAddFolderState)
    }

    @Test
    fun `GIVEN current screen is edit bookmark WHEN back is clicked THEN navigate back, save the bookmark, and load the updated tree`() = runTest {
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(generateBookmarkTree())
        val middleware = buildMiddleware()
        val store = middleware.makeStore()
        val newBookmarkTitle = "my awesome bookmark"

        val bookmark = store.state.bookmarkItems.first { it is BookmarkItem.Bookmark } as BookmarkItem.Bookmark
        store.dispatch(EditBookmarkClicked(bookmark = bookmark))
        store.dispatch(EditBookmarkAction.TitleChanged(title = newBookmarkTitle))

        assertNotNull(store.state.bookmarksEditBookmarkState)
        store.dispatch(BackClicked)

        verify(bookmarksStorage).updateNode(
            guid = "item guid 0",
            info = BookmarkInfo(
                parentGuid = BookmarkRoot.Mobile.id,
                position = 5u,
                title = "my awesome bookmark",
                url = "item url 0",
            ),
        )
        verify(bookmarksStorage, times(2)).getTree(BookmarkRoot.Mobile.id)
        verify(navController).popBackStack()
        assertNull(store.state.bookmarksEditBookmarkState)
    }

    @Test
    fun `GIVEN current screen is list and the top-level is loaded WHEN back is clicked THEN exit bookmarks`() = runTestOnMain {
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(generateBookmarkTree())
        var exited = false
        exitBookmarks = { exited = true }
        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        store.dispatch(BackClicked)

        assertTrue(exited)
    }

    @Test
    fun `GIVEN current screen is an empty list and the top-level is loaded WHEN sign into sync is clicked THEN navigate to sign into sync `() = runTestOnMain {
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(generateBookmarkTree())
        var navigated = false
        navigateToSignIntoSync = { navigated = true }
        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        store.dispatch(SignIntoSyncClicked)

        assertTrue(navigated)
    }

    @Test
    fun `GIVEN current screen is list and a sub-level folder is loaded WHEN back is clicked THEN load the parent level`() = runTestOnMain {
        val tree = generateBookmarkTree()
        val firstFolderNode = tree.children!!.first { it.type == BookmarkNodeType.FOLDER }
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(tree)
        `when`(bookmarksStorage.getTree(firstFolderNode.guid)).thenReturn(generateBookmarkTree())
        `when`(bookmarksStorage.getBookmark(firstFolderNode.guid)).thenReturn(firstFolderNode)
        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        store.dispatch(FolderClicked(BookmarkItem.Folder(title = firstFolderNode.title!!, guid = firstFolderNode.guid)))

        assertEquals(firstFolderNode.guid, store.state.currentFolder.guid)
        store.dispatch(BackClicked)

        assertEquals(BookmarkRoot.Mobile.id, store.state.currentFolder.guid)
        assertEquals(tree.children!!.size, store.state.bookmarkItems.size)
    }

    @Test
    fun `GIVEN bookmarks in storage WHEN select folder sub screen view is loaded THEN load folders into sub screen state`() = runTestOnMain {
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id, recursive = true)).thenReturn(generateBookmarkTree())
        val middleware = buildMiddleware()
        val store = middleware.makeStore(
            initialState = BookmarksState.default.copy(
                bookmarksSelectFolderState = BookmarksSelectFolderState(),
            ),
        )

        store.dispatch(SelectFolderAction.ViewAppeared)

        assertEquals(6, store.state.bookmarksSelectFolderState?.folders?.count())
    }

    private fun buildMiddleware() = BookmarksMiddleware(
        bookmarksStorage = bookmarksStorage,
        navController = navController,
        exitBookmarks = exitBookmarks,
        navigateToSignIntoSync = navigateToSignIntoSync,
        resolveFolderTitle = resolveFolderTitle,
        getBrowsingMode = getBrowsingMode,
        openTab = openTab,
        ioDispatcher = coroutineRule.testDispatcher,
    )

    private fun BookmarksMiddleware.makeStore(
        initialState: BookmarksState = BookmarksState.default,
    ) = BookmarksStore(
        initialState = initialState,
        middleware = listOf(this),
    ).also {
        it.waitUntilIdle()
    }

    private fun generateBookmarkFolders(parentGuid: String) = List(5) {
        generateBookmarkFolder(
            guid = "folder guid $it",
            title = "folder title $it",
            parentGuid = parentGuid,
        )
    }

    private val bookmarkItems = List(5) {
        generateBookmark("item guid $it", "item title $it", "item url $it")
    }

    private fun generateBookmarkTree() = BookmarkNode(
        type = BookmarkNodeType.FOLDER,
        guid = BookmarkRoot.Mobile.id,
        parentGuid = null,
        position = 0U,
        title = "mobile",
        url = null,
        dateAdded = 0L,
        children = generateBookmarkFolders(BookmarkRoot.Mobile.id) + bookmarkItems,
    )

    private fun generateBookmarkFolder(guid: String, title: String, parentGuid: String) = BookmarkNode(
        type = BookmarkNodeType.FOLDER,
        guid = guid,
        parentGuid = parentGuid,
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
