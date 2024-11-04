/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

import android.content.ClipboardManager
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
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.any
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
import org.mozilla.fenix.GleanMetrics.CustomizeHome.bookmarks
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.library.bookmarks.friendlyRootTitle

@RunWith(AndroidJUnit4::class)
class BookmarksMiddlewareTest {

    @get:Rule
    val coroutineRule = MainCoroutineRule()

    private lateinit var bookmarksStorage: BookmarksStorage
    private lateinit var clipboardManager: ClipboardManager
    private lateinit var addNewTabUseCase: TabsUseCases.AddNewTabUseCase
    private lateinit var navController: NavController
    private lateinit var navigateToSignIntoSync: () -> Unit
    private lateinit var exitBookmarks: () -> Unit
    private lateinit var wasPreviousAppDestinationHome: () -> Boolean
    private lateinit var navigateToSearch: () -> Unit
    private lateinit var shareBookmark: (String, String) -> Unit
    private lateinit var showTabsTray: (Boolean) -> Unit
    private lateinit var showUrlCopiedSnackbar: () -> Unit
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
        clipboardManager = mock()
        addNewTabUseCase = mock()
        navController = mock()
        navigateToSignIntoSync = { }
        exitBookmarks = { }
        wasPreviousAppDestinationHome = { false }
        navigateToSearch = { }
        shareBookmark = { _, _ -> }
        showTabsTray = { _ -> }
        showUrlCopiedSnackbar = { }
        getBrowsingMode = { BrowsingMode.Normal }
        openTab = { _, _ -> }
    }

    @Test
    fun `GIVEN bookmarks in storage and not signed into sync WHEN store is initialized THEN bookmarks will be loaded as display format`() = runTestOnMain {
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(generateBookmarkTree())
        val middleware = buildMiddleware()

        val store = middleware.makeStore()

        assertEquals(10, store.state.bookmarkItems.size)
    }

    @Test
    fun `GIVEN bookmarks in storage and not signed into sync WHEN store is initialized THEN bookmarks will be sorted by last modified date`() = runTestOnMain {
        val reverseOrderByModifiedBookmarks = List(5) {
            generateBookmark(
                guid = "$it",
                title = "$it",
                url = "$it",
                lastModified = it.toLong(),
            )
        }
        val root = BookmarkNode(
            type = BookmarkNodeType.FOLDER,
            guid = BookmarkRoot.Mobile.id,
            parentGuid = null,
            position = 0U,
            title = "mobile",
            url = null,
            dateAdded = 0,
            lastModified = 0,
            children = reverseOrderByModifiedBookmarks,
        )
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(root)
        val middleware = buildMiddleware()

        val store = middleware.makeStore()

        val bookmarksConvertedToSortedItems = reverseOrderByModifiedBookmarks
            .map {
                BookmarkItem.Bookmark(url = it.url!!, title = it.title!!, previewImageUrl = it.url!!, guid = it.guid)
            }
            .reversed()
        assertEquals(5, store.state.bookmarkItems.size)
        assertEquals(bookmarksConvertedToSortedItems, store.state.bookmarkItems)
    }

    @Test
    fun `GIVEN bookmarks in storage and user has a desktop bookmark WHEN store is initialized THEN bookmarks, including desktop will be loaded as display format`() = runTestOnMain {
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(1u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(generateBookmarkTree())
        `when`(bookmarksStorage.getTree(BookmarkRoot.Root.id)).thenReturn(generateDesktopRootTree())
        val middleware = buildMiddleware()

        val store = middleware.makeStore(
            initialState = BookmarksState.default.copy(isSignedIntoSync = true),
        )

        assertEquals(11, store.state.bookmarkItems.size)
    }

    @Test
    fun `GIVEN bookmarks in storage and not signed into sync but has existing desktop bookmarks WHEN store is initialized THEN bookmarks, including desktop will be loaded as display format`() = runTestOnMain {
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(1u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(generateBookmarkTree())
        `when`(bookmarksStorage.getTree(BookmarkRoot.Root.id)).thenReturn(generateDesktopRootTree())
        val middleware = buildMiddleware()

        val store = middleware.makeStore()

        assertEquals(11, store.state.bookmarkItems.size)
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
        getBrowsingMode = { BrowsingMode.Normal }
        var capturedUrl = ""
        var capturedNewTab = false
        openTab = { urlCalled, newTab ->
            capturedUrl = urlCalled
            capturedNewTab = newTab
        }
        wasPreviousAppDestinationHome = { true }

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
        wasPreviousAppDestinationHome = { false }

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
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
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
        var navigated = false
        navigateToSearch = { navigated = true }
        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        store.dispatch(SearchClicked)

        assertTrue(navigated)
    }

    @Test
    fun `WHEN add folder button is clicked THEN navigate to folder screen`() {
        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        store.dispatch(AddFolderClicked)

        verify(navController).navigate(BookmarksDestinations.ADD_FOLDER)
    }

    @Test
    fun `GIVEN current screen is add folder WHEN parent folder is clicked THEN navigate to folder selection screen`() {
        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        store.dispatch(AddFolderAction.ParentFolderClicked)

        verify(navController).navigate(BookmarksDestinations.SELECT_FOLDER)
    }

    @Test
    fun `GIVEN current screen is edit bookmark WHEN folder is clicked THEN navigate to folder selection screen`() {
        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        store.dispatch(EditBookmarkAction.FolderClicked)

        verify(navController).navigate(BookmarksDestinations.SELECT_FOLDER)
    }

    @Test
    fun `GIVEN current screen is add folder and new folder title is nonempty WHEN back is clicked THEN navigate back, save the new folder, and load the updated tree`() = runTest {
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(generateBookmarkTree())
        `when`(bookmarksStorage.addFolder(BookmarkRoot.Mobile.id, "test")).thenReturn("new-guid")

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
    fun `GIVEN current screen is add folder and previous screen is select folder WHEN back is clicked THEN navigate back to the edit bookmark screen`() = runTestOnMain {
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id, recursive = true)).thenReturn(generateBookmarkTree())
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id, recursive = false)).thenReturn(generateBookmarkTree())
        `when`(bookmarksStorage.addFolder(BookmarkRoot.Mobile.id, "i'm a new folder")).thenReturn("new-guid")
        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        val bookmark = store.state.bookmarkItems.first { it is BookmarkItem.Bookmark } as BookmarkItem.Bookmark
        val newFolderTitle = "i'm a new folder"

        store.dispatch(BookmarksListMenuAction.Bookmark.EditClicked(bookmark))
        store.dispatch(AddFolderAction.ParentFolderClicked)
        store.dispatch(SelectFolderAction.ViewAppeared)
        store.dispatch(AddFolderClicked)
        store.dispatch(AddFolderAction.TitleChanged(newFolderTitle))
        store.dispatch(BackClicked)
        store.waitUntilIdle()

        assertNull(store.state.bookmarksSelectFolderState)
        verify(bookmarksStorage, times(1)).getTree(BookmarkRoot.Mobile.id, recursive = true)
        verify(navController, times(1)).popBackStack(BookmarksDestinations.EDIT_BOOKMARK, inclusive = false)
    }

    @Test
    fun `GIVEN current screen is add folder and previous screen is not select folder WHEN back is clicked THEN navigate back`() = runTestOnMain {
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id, recursive = false)).thenReturn(generateBookmarkTree())
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id, recursive = false)).thenReturn(generateBookmarkTree())
        `when`(bookmarksStorage.addFolder(BookmarkRoot.Mobile.id, "i'm a new folder")).thenReturn("new-guid")
        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        store.dispatch(AddFolderClicked)
        store.dispatch(AddFolderAction.TitleChanged("i'm a new folder"))
        store.dispatch(BackClicked)
        store.waitUntilIdle()

        verify(bookmarksStorage, times(2)).getTree(BookmarkRoot.Mobile.id, recursive = false)
        verify(navController, times(1)).popBackStack()
    }

    @Test
    fun `GIVEN current screen is edit folder and new title is nonempty WHEN back is clicked THEN navigate back, save the folder, and load the updated tree`() = runTest {
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(generateBookmarkTree())
        val middleware = buildMiddleware()
        val store = middleware.makeStore(
            initialState = BookmarksState.default.copy(
                bookmarksEditFolderState = BookmarksEditFolderState(
                    parent = BookmarkItem.Folder("Bookmarks", "guid0"),
                    folder = BookmarkItem.Folder("folder title 0", "folder guid 0"),
                ),
            ),
        )

        val newFolderTitle = "test"

        store.dispatch(EditFolderAction.TitleChanged(newFolderTitle))
        store.dispatch(BackClicked)

        verify(bookmarksStorage).updateNode(
            guid = "folder guid 0",
            info = BookmarkInfo(
                parentGuid = "guid0",
                position = 0u,
                title = "test",
                url = null,
            ),
        )
        verify(bookmarksStorage, times(2)).getTree(BookmarkRoot.Mobile.id)
        verify(navController).popBackStack()
        assertNull(store.state.bookmarksEditFolderState)
    }

    @Test
    fun `GIVEN current screen is edit folder and new title is empty WHEN back is clicked THEN navigate back, without siving the folder, and load the updated tree`() = runTest {
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(generateBookmarkTree())
        val middleware = buildMiddleware()
        val store = middleware.makeStore(
            initialState = BookmarksState.default.copy(
                bookmarksEditFolderState = BookmarksEditFolderState(
                    parent = BookmarkItem.Folder("Bookmarks", "guid0"),
                    folder = BookmarkItem.Folder("folder title 0", "folder guid 0"),
                ),
            ),
        )

        val newFolderTitle = ""

        store.dispatch(EditFolderAction.TitleChanged(newFolderTitle))
        store.dispatch(BackClicked)

        verify(bookmarksStorage, never()).updateNode(
            guid = "folder guid 0",
            info = BookmarkInfo(
                parentGuid = "guid0",
                position = 0u,
                title = "test",
                url = null,
            ),
        )
        verify(bookmarksStorage, times(2)).getTree(BookmarkRoot.Mobile.id)
        verify(navController).popBackStack()
        assertNull(store.state.bookmarksEditFolderState)
    }

    @Test
    fun `GIVEN current screen is edit bookmark WHEN back is clicked THEN navigate back, save the bookmark, and load the updated tree`() = runTest {
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
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
    fun `GIVEN current screen is edit bookmark and the bookmark title is empty WHEN back is clicked THEN navigate back`() = runTest {
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(generateBookmarkTree())
        val middleware = buildMiddleware()
        val store = middleware.makeStore()
        val newBookmarkTitle = ""

        val bookmark = store.state.bookmarkItems.first { it is BookmarkItem.Bookmark } as BookmarkItem.Bookmark
        store.dispatch(EditBookmarkClicked(bookmark = bookmark))
        store.dispatch(EditBookmarkAction.TitleChanged(title = newBookmarkTitle))

        assertNotNull(store.state.bookmarksEditBookmarkState)
        store.dispatch(BackClicked)

        verify(bookmarksStorage, never()).updateNode(
            guid = "item guid 0",
            info = BookmarkInfo(
                parentGuid = BookmarkRoot.Mobile.id,
                position = 5u,
                title = "",
                url = "item url 0",
            ),
        )

        verify(navController).popBackStack()
        assertNull(store.state.bookmarksEditBookmarkState)
    }

    @Test
    fun `GIVEN current screen is list and the top-level is loaded WHEN back is clicked THEN exit bookmarks`() = runTestOnMain {
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
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
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(generateBookmarkTree())
        var navigated = false
        navigateToSignIntoSync = { navigated = true }
        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        store.dispatch(SignIntoSyncClicked)

        assertTrue(navigated)
    }

    @Test
    fun `GIVEN current screen is a subfolder WHEN close is clicked THEN exit bookmarks `() = runTestOnMain {
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(generateBookmarkTree())
        var navigated = false
        exitBookmarks = { navigated = true }
        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        store.dispatch(CloseClicked)

        assertTrue(navigated)
    }

    @Test
    fun `GIVEN current screen is list and a sub-level folder is loaded WHEN back is clicked THEN load the parent level`() = runTestOnMain {
        val tree = generateBookmarkTree()
        val firstFolderNode = tree.children!!.first { it.type == BookmarkNodeType.FOLDER }
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
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
    fun `GIVEN bookmarks in storage and not signed into sync WHEN select folder sub screen view is loaded THEN load folders into sub screen state`() = runTestOnMain {
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id, recursive = true)).thenReturn(generateBookmarkTree())
        val middleware = buildMiddleware()
        val store = middleware.makeStore(
            initialState = BookmarksState.default.copy(
                bookmarksSelectFolderState = BookmarksSelectFolderState(outerSelectionGuid = "selection guid"),
            ),
        )

        store.dispatch(SelectFolderAction.ViewAppeared)

        assertEquals(6, store.state.bookmarksSelectFolderState?.folders?.count())
    }

    @Test
    fun `GIVEN bookmarks in storage and not signed into sync but have pre-existing desktop bookmarks saved WHEN select folder sub screen view is loaded THEN load folders, including desktop folders into sub screen state`() = runTestOnMain {
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(1u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Root.id, recursive = true)).thenReturn(generateDesktopRootTree())
        val middleware = buildMiddleware()
        val store = middleware.makeStore(
            initialState = BookmarksState.default.copy(
                bookmarksSelectFolderState = BookmarksSelectFolderState(outerSelectionGuid = "selection guid"),
            ),
        )

        store.dispatch(SelectFolderAction.ViewAppeared)

        assertEquals(10, store.state.bookmarksSelectFolderState?.folders?.count())
    }

    @Test
    fun `GIVEN bookmarks in storage and has desktop bookmarks WHEN select folder sub screen view is loaded THEN load folders, including desktop folders into sub screen state`() = runTestOnMain {
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(1u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Root.id, recursive = true)).thenReturn(generateDesktopRootTree())
        val middleware = buildMiddleware()
        val store = middleware.makeStore(
            initialState = BookmarksState.default.copy(
                isSignedIntoSync = true,
                bookmarksSelectFolderState = BookmarksSelectFolderState(outerSelectionGuid = "selection guid"),
            ),
        )

        store.dispatch(SelectFolderAction.ViewAppeared)

        assertEquals(10, store.state.bookmarksSelectFolderState?.folders?.count())
    }

    @Test
    fun `GIVEN current screen select folder WHEN back is clicked THEN pop the backstack`() = runTestOnMain {
        val middleware = buildMiddleware()
        val store = middleware.makeStore(
            initialState = BookmarksState.default.copy(
                bookmarksSelectFolderState = BookmarksSelectFolderState(outerSelectionGuid = "selection guid"),
            ),
        )

        store.dispatch(BackClicked)
        verify(navController).popBackStack()
    }

    @Test
    fun `GIVEN current screen select folder while multi-selecting WHEN back is clicked THEN pop the backstack and update the selected bookmark items`() = runTestOnMain {
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(generateBookmarkTree())
        val middleware = buildMiddleware()
        val store = middleware.makeStore(
            initialState = BookmarksState.default.copy(
                bookmarksMultiselectMoveState = MultiselectMoveState(
                    guidsToMove = listOf("item guid 1", "item guid 2"),
                    destination = "folder guid 1",
                ),
                bookmarksSelectFolderState = BookmarksSelectFolderState(
                    outerSelectionGuid = "folder guid 1",
                ),
            ),
        )

        store.dispatch(BackClicked)

        verify(bookmarksStorage, times(2)).getTree(BookmarkRoot.Mobile.id)
        verify(navController).popBackStack()
        verify(bookmarksStorage).updateNode(
            guid = "item guid 1",
            info = BookmarkInfo(
                parentGuid = "folder guid 1",
                position = null,
                title = "item title 1",
                url = "item url 1",
            ),
        )
        verify(bookmarksStorage).updateNode(
            guid = "item guid 2",
            info = BookmarkInfo(
                parentGuid = "folder guid 1",
                position = null,
                title = "item title 2",
                url = "item url 2",
            ),
        )
    }

    @Test
    fun `WHEN edit clicked in bookmark item menu THEN nav to edit screen`() = runTestOnMain {
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(generateBookmarkTree())
        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        val bookmark = store.state.bookmarkItems.first { it is BookmarkItem.Bookmark } as BookmarkItem.Bookmark
        store.dispatch(BookmarksListMenuAction.Bookmark.EditClicked(bookmark))

        assertEquals(bookmark, store.state.bookmarksEditBookmarkState!!.bookmark)
        verify(navController).navigate(BookmarksDestinations.EDIT_BOOKMARK)
    }

    @Test
    fun `WHEN copy clicked in bookmark item menu THEN copy bookmark url to clipboard and snackboard is shown`() {
        val url = "url"
        val bookmarkItem = BookmarkItem.Bookmark(url = url, title = "title", previewImageUrl = url, guid = "guid")
        var snackShown = false
        showUrlCopiedSnackbar = { snackShown = true }
        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        store.dispatch(BookmarksListMenuAction.Bookmark.CopyClicked(bookmarkItem))

        verify(clipboardManager).setPrimaryClip(any())
        assertTrue(snackShown)
    }

    @Test
    fun `WHEN share clicked in bookmark item menu THEN share the bookmark`() {
        var resultUrl = ""
        var resultTitle = ""
        shareBookmark = { sharedUrl, sharedTitle ->
            resultUrl = sharedUrl
            resultTitle = sharedTitle
        }
        val middleware = buildMiddleware()
        val store = middleware.makeStore()
        val url = "url"
        val title = "title"
        val bookmarkItem = BookmarkItem.Bookmark(url = url, title = title, previewImageUrl = url, guid = "guid")

        store.dispatch(BookmarksListMenuAction.Bookmark.ShareClicked(bookmarkItem))
        assertEquals(url, resultUrl)
        assertEquals(title, resultTitle)
    }

    @Test
    fun `WHEN open in normal tab clicked in bookmark item menu THEN add a normal tab and show the tabs tray in normal mode`() {
        val url = "url"
        val bookmarkItem = BookmarkItem.Bookmark(url = url, title = "title", previewImageUrl = url, guid = "guid")
        var trayShown = false
        var mode = true
        showTabsTray = { newMode ->
            mode = newMode
            trayShown = true
        }
        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        store.dispatch(BookmarksListMenuAction.Bookmark.OpenInNormalTabClicked(bookmarkItem))

        verify(addNewTabUseCase).invoke(url = url, private = false)
        assertTrue(trayShown)
        assertFalse(mode)
    }

    @Test
    fun `WHEN open in private tab clicked in bookmark item menu THEN add a private tab and show the tabs tray in private mode`() {
        val url = "url"
        val bookmarkItem = BookmarkItem.Bookmark(url = url, title = "title", previewImageUrl = url, guid = "guid")
        var trayShown = false
        var mode = false
        showTabsTray = { newMode ->
            mode = newMode
            trayShown = true
        }
        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        store.dispatch(BookmarksListMenuAction.Bookmark.OpenInPrivateTabClicked(bookmarkItem))

        verify(addNewTabUseCase).invoke(url = url, private = true)
        assertTrue(trayShown)
        assertTrue(mode)
    }

    @Test
    fun `GIVEN a state with an undo snackbar WHEN snackbar is dismissed THEN delete all of the guids`() = runTestOnMain {
        val tree = generateBookmarkTree()
        val firstGuid = tree.children!!.first().guid
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(tree)
        val bookmarkItem = BookmarkItem.Bookmark(url = "url", title = "title", previewImageUrl = "url", guid = firstGuid)
        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        store.dispatch(BookmarksListMenuAction.Bookmark.DeleteClicked(bookmarkItem))
        assertEquals(BookmarksSnackbarState.UndoDeletion(listOf(firstGuid)), store.state.bookmarksSnackbarState)

        val initialCount = store.state.bookmarkItems.size
        store.dispatch(SnackbarAction.Dismissed)
        assertEquals(BookmarksSnackbarState.None, store.state.bookmarksSnackbarState)
        assertEquals(initialCount - 1, store.state.bookmarkItems.size)

        verify(bookmarksStorage).deleteNode(firstGuid)
    }

    @Test
    fun `GIVEN a user is on the edit screen with nothing on the backstack WHEN delete is clicked THEN pop the backstack, delete the bookmark and exit bookmarks`() = runTestOnMain {
        var exited = false
        exitBookmarks = { exited = true }
        val middleware = buildMiddleware()
        val store = middleware.makeStore(
            initialState = BookmarksState.default.copy(
                bookmarksEditBookmarkState = BookmarksEditBookmarkState(
                    bookmark = BookmarkItem.Bookmark("ur", "title", "url", "guid"),
                    folder = BookmarkItem.Folder("title", "guid"),
                ),
            ),
        )
        `when`(navController.popBackStack()).thenReturn(false)
        store.dispatch(EditBookmarkAction.DeleteClicked)
        verify(navController).popBackStack()
        verify(bookmarksStorage).deleteNode("guid")
        assertTrue(exited)
    }

    @Test
    fun `WHEN edit clicked in folder item menu THEN nav to the edit screen`() {
        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        store.dispatch(BookmarksListMenuAction.Folder.EditClicked(folder = store.state.currentFolder))

        verify(navController).navigate(BookmarksDestinations.EDIT_FOLDER)
    }

    @Test
    fun `GIVEN a folder with fewer than 15 items WHEN open all in normal tabs clicked in folder item menu THEN open all the bookmarks as normal tabs and show the tabs tray in normal mode`() = runTestOnMain {
        val guid = "guid"
        val folderItem = BookmarkItem.Folder(title = "title", guid = guid)
        val folder = generateBookmarkFolder(guid = guid, "title", "parentGuid")
        `when`(bookmarksStorage.getTree(guid)).thenReturn(folder)
        var trayShown = false
        var mode = true
        showTabsTray = { newMode ->
            mode = newMode
            trayShown = true
        }
        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        store.dispatch(BookmarksListMenuAction.Folder.OpenAllInNormalTabClicked(folderItem))

        folder.children!!.forEach { child ->
            verify(addNewTabUseCase).invoke(url = child.url!!, private = false)
        }
        assertTrue(trayShown)
        assertFalse(mode)
    }

    @Test
    fun `GIVEN a folder with 15 or more items WHEN open all in normal tabs clicked in folder item menu THEN show a warning`() = runTestOnMain {
        val guid = "guid"
        val folderItem = BookmarkItem.Folder(title = "title", guid = guid)
        val folder = generateBookmarkFolder(guid = guid, "title", "parentGuid").copy(
            children = List(15) {
                generateBookmark(
                    guid = "bookmark guid $it",
                    title = "bookmark title $it",
                    url = "bookmark urk",
                )
            },
        )
        `when`(bookmarksStorage.getTree(guid)).thenReturn(folder)
        var trayShown = false
        showTabsTray = { _ -> trayShown = true }
        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        store.dispatch(BookmarksListMenuAction.Folder.OpenAllInNormalTabClicked(folderItem))
        val expected = OpenTabsConfirmationDialog.Presenting(
            guidToOpen = guid,
            numberOfTabs = 15,
            isPrivate = false,
        )
        assertEquals(expected, store.state.openTabsConfirmationDialog)

        folder.children!!.forEach { child ->
            verify(addNewTabUseCase, never()).invoke(url = child.url!!, private = false)
        }
        assertFalse(trayShown)
    }

    @Test
    fun `GIVEN a folder with fewer than 15 items WHEN open all in private tabs clicked in folder item menu THEN open all the bookmarks as private tabs and show the tabs tray in private mode`() = runTestOnMain {
        val guid = "guid"
        val folderItem = BookmarkItem.Folder(title = "title", guid = guid)
        val folder = generateBookmarkFolder(guid = guid, "title", "parentGuid")
        `when`(bookmarksStorage.getTree(guid)).thenReturn(folder)
        var trayShown = false
        var mode = false
        showTabsTray = { newMode ->
            mode = newMode
            trayShown = true
        }
        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        store.dispatch(BookmarksListMenuAction.Folder.OpenAllInPrivateTabClicked(folderItem))

        folder.children!!.forEach { child ->
            verify(addNewTabUseCase).invoke(url = child.url!!, private = true)
        }
        assertTrue(trayShown)
        assertTrue(mode)
    }

    @Test
    fun `GIVEN a folder with 15 or more items WHEN open all in private tabs clicked in folder item menu THEN show a warning`() = runTestOnMain {
        val guid = "guid"
        val folderItem = BookmarkItem.Folder(title = "title", guid = guid)
        val folder = generateBookmarkFolder(guid = guid, "title", "parentGuid").copy(
            children = List(15) {
                generateBookmark(
                    guid = "bookmark guid $it",
                    title = "bookmark title $it",
                    url = "bookmark urk",
                )
            },
        )
        `when`(bookmarksStorage.getTree(guid)).thenReturn(folder)
        var trayShown = false
        showTabsTray = { _ -> trayShown = true }
        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        store.dispatch(BookmarksListMenuAction.Folder.OpenAllInPrivateTabClicked(folderItem))
        val expected = OpenTabsConfirmationDialog.Presenting(
            guidToOpen = guid,
            numberOfTabs = 15,
            isPrivate = true,
        )
        assertEquals(expected, store.state.openTabsConfirmationDialog)

        folder.children!!.forEach { child ->
            verify(addNewTabUseCase, never()).invoke(url = child.url!!, private = true)
        }
        assertFalse(trayShown)
    }

    @Test
    fun `WHEN delete clicked in folder item menu THEN present a dialog showing the number of items to be deleted and when delete clicked, delete the selected folder`() = runTestOnMain {
        val tree = generateBookmarkTree()
        val folder = tree.children!!.first { it.type == BookmarkNodeType.FOLDER }
        val folderItem = BookmarkItem.Folder(guid = folder.guid, title = "title")
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(folderItem.guid))).thenReturn(19u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(tree)
        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        store.dispatch(BookmarksListMenuAction.Folder.DeleteClicked(folderItem))
        assertEquals(DeletionDialogState.Presenting(listOf(folderItem.guid), 19), store.state.bookmarksDeletionDialogState)

        store.dispatch(DeletionDialogAction.DeleteTapped)
        assertEquals(DeletionDialogState.None, store.state.bookmarksDeletionDialogState)
        verify(bookmarksStorage).deleteNode(folder.guid)
    }

    @Test
    fun `WHEN delete clicked in folder edit screen THEN present a dialog showing the number of items to be deleted and when delete clicked, delete the selected folder`() = runTestOnMain {
        val tree = generateBookmarkTree()
        val folder = tree.children!!.first { it.type == BookmarkNodeType.FOLDER }
        val folderItem = BookmarkItem.Folder(guid = folder.guid, title = "title")
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(folderItem.guid))).thenReturn(19u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(tree)
        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        store.dispatch(BookmarksListMenuAction.Folder.EditClicked(folderItem))
        store.dispatch(EditFolderAction.DeleteClicked)
        assertEquals(DeletionDialogState.Presenting(listOf(folderItem.guid), 19), store.state.bookmarksDeletionDialogState)

        store.dispatch(DeletionDialogAction.DeleteTapped)
        assertEquals(DeletionDialogState.None, store.state.bookmarksDeletionDialogState)
        verify(bookmarksStorage).deleteNode(folder.guid)
        verify(navController).popBackStack()
    }

    @Test
    fun `WHEN toolbar edit clicked THEN navigate to the edit screen`() = runTestOnMain {
        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        store.dispatch(BookmarksListMenuAction.MultiSelect.EditClicked)

        verify(navController).navigate(BookmarksDestinations.EDIT_BOOKMARK)
    }

    @Test
    fun `GIVEN selected tabs WHEN multi-select open in normal tabs clicked THEN open selected in new tabs and show tabs tray`() = runTestOnMain {
        var shown = false
        var mode = true
        showTabsTray = { newMode ->
            shown = true
            mode = newMode
        }
        val tree = generateBookmarkTree()
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(tree)
        val items = tree.children!!.filter { it.type == BookmarkNodeType.ITEM }.take(2).map {
            BookmarkItem.Bookmark(guid = it.guid, title = it.title!!, url = it.url!!, previewImageUrl = it.url!!)
        }
        val middleware = buildMiddleware()
        val store = middleware.makeStore(
            initialState = BookmarksState.default.copy(selectedItems = items),
        )

        store.dispatch(BookmarksListMenuAction.MultiSelect.OpenInNormalTabsClicked)

        assertTrue(items.size == 2)
        for (item in items) {
            verify(addNewTabUseCase).invoke(item.url, private = false)
        }
        assertTrue(shown)
        assertFalse(mode)
    }

    @Test
    fun `GIVEN selected tabs WHEN multi-select open in private tabs clicked THEN open selected in new private tabs and show tabs tray`() = runTestOnMain {
        var shown = false
        var mode = false
        showTabsTray = { newMode ->
            shown = true
            mode = newMode
        }
        val tree = generateBookmarkTree()
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(tree)
        val items = tree.children!!.filter { it.type == BookmarkNodeType.ITEM }.take(2).map {
            BookmarkItem.Bookmark(guid = it.guid, title = it.title!!, url = it.url!!, previewImageUrl = it.url!!)
        }
        val middleware = buildMiddleware()
        val store = middleware.makeStore(
            initialState = BookmarksState.default.copy(selectedItems = items),
        )

        store.dispatch(BookmarksListMenuAction.MultiSelect.OpenInPrivateTabsClicked)

        assertTrue(items.size == 2)
        for (item in items) {
            verify(addNewTabUseCase).invoke(item.url, private = true)
        }
        assertTrue(shown)
        assertTrue(mode)
    }

    @Test
    fun `GIVEN selected tabs WHEN multi-select share clicked THEN share all tabs`() = runTestOnMain {
        val sharedUrls = mutableListOf<String>()
        val sharedTitles = mutableListOf<String>()
        shareBookmark = { url, title ->
            sharedUrls.add(url)
            sharedTitles.add(title)
        }
        val tree = generateBookmarkTree()
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(tree)
        val items = tree.children!!.filter { it.type == BookmarkNodeType.ITEM }.take(2).map {
            BookmarkItem.Bookmark(guid = it.guid, title = it.title!!, url = it.url!!, previewImageUrl = it.url!!)
        }
        val middleware = buildMiddleware()
        val store = middleware.makeStore(
            initialState = BookmarksState.default.copy(selectedItems = items),
        )

        store.dispatch(BookmarksListMenuAction.MultiSelect.ShareClicked)

        assertTrue(items.size == 2)
        for (item in items) {
            assertTrue(item.url in sharedUrls)
            assertTrue(item.title in sharedTitles)
        }
    }

    @Test
    fun `GIVEN a single item selected WHEN multi-select delete clicked THEN show snackbar`() = runTestOnMain {
        val tree = generateBookmarkTree()
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(tree)
        val bookmarkItem = tree.children!!.first { it.type == BookmarkNodeType.ITEM }.let {
            BookmarkItem.Bookmark(guid = it.guid, title = it.title!!, url = it.url!!, previewImageUrl = it.url!!)
        }

        val middleware = buildMiddleware()
        val store = middleware.makeStore(
            initialState = BookmarksState.default.copy(selectedItems = listOf(bookmarkItem)),
        )

        store.dispatch(BookmarksListMenuAction.MultiSelect.DeleteClicked)
        assertEquals(BookmarksSnackbarState.UndoDeletion(listOf(bookmarkItem.guid)), store.state.bookmarksSnackbarState)

        val initialCount = store.state.bookmarkItems.size
        store.dispatch(SnackbarAction.Dismissed)
        assertEquals(BookmarksSnackbarState.None, store.state.bookmarksSnackbarState)
        assertEquals(initialCount - 1, store.state.bookmarkItems.size)

        verify(bookmarksStorage).deleteNode(bookmarkItem.guid)
    }

    @Test
    fun `GIVEN multiple selected items WHEN multi-select delete clicked THEN show the confirmation dialog`() = runTestOnMain {
        val tree = generateBookmarkTree()
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(tree)
        val items = tree.children!!.filter { it.type == BookmarkNodeType.ITEM }.take(2).map {
            BookmarkItem.Bookmark(guid = it.guid, title = it.title!!, url = it.url!!, previewImageUrl = it.url!!)
        }
        val middleware = buildMiddleware()
        val store = middleware.makeStore(
            initialState = BookmarksState.default.copy(selectedItems = items),
        )
        `when`(bookmarksStorage.countBookmarksInTrees(items.map { it.guid })).thenReturn(19u)
        store.dispatch(BookmarksListMenuAction.MultiSelect.DeleteClicked)
        assertEquals(DeletionDialogState.Presenting(items.map { it.guid }, 19), store.state.bookmarksDeletionDialogState)

        val initialCount = store.state.bookmarkItems.size
        store.dispatch(DeletionDialogAction.DeleteTapped)
        assertEquals(DeletionDialogState.None, store.state.bookmarksDeletionDialogState)
        assertEquals(initialCount - 2, store.state.bookmarkItems.size)

        for (item in items) {
            verify(bookmarksStorage).deleteNode(item.guid)
        }
    }

    @Test
    fun `GIVEN selected items in state WHEN a folder is clicked THEN update the recursive state`() = runTestOnMain {
        val tree = generateBookmarkTree()
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(tree)
        val middleware = buildMiddleware()
        val store = middleware.makeStore(
            initialState = BookmarksState.default.copy(
                selectedItems = listOf(BookmarkItem.Folder("Folder 1", "guid1")),
            ),
        )
        `when`(bookmarksStorage.countBookmarksInTrees(listOf("guid1", "guid2"))).thenReturn(19u)
        store.dispatch(FolderClicked(BookmarkItem.Folder("Folder2", "guid2")))
        assertEquals(19, store.state.recursiveSelectedCount)
    }

    @Test
    fun `GIVEN selected items in state WHEN move folder is clicked THEN navigate to folder selection`() = runTestOnMain {
        val tree = generateBookmarkTree()
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(tree)
        val middleware = buildMiddleware()
        val store = middleware.makeStore(
            initialState = BookmarksState.default.copy(
                selectedItems = listOf(BookmarkItem.Folder("Folder 1", "guid1")),
            ),
        )

        store.dispatch(BookmarksListMenuAction.MultiSelect.MoveClicked)
        verify(navController).navigate(BookmarksDestinations.SELECT_FOLDER)
    }

    @Test
    fun `WHEN first bookmarks sync is complete THEN reload the bookmarks list`() = runTestOnMain {
        val syncedGuid = "sync"
        val tree = generateBookmarkTree()
        val afterSyncTree = tree.copy(children = tree.children?.plus(generateBookmark(guid = syncedGuid, "title", "url")))
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id))
            .thenReturn(tree)
            .thenReturn(afterSyncTree)
        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        store.dispatch(FirstSyncCompleted)

        assertTrue(store.state.bookmarkItems.any { it.guid == syncedGuid })
    }

    @Test
    fun `GIVEN a bookmark has been deleted WHEN the view is disposed before the snackbar is dismissed THEN commit the deletion`() = runTestOnMain {
        val tree = generateBookmarkTree()
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(tree)

        val middleware = buildMiddleware()
        val store = middleware.makeStore()
        val bookmarkToDelete = store.state.bookmarkItems.first { it is BookmarkItem.Bookmark } as BookmarkItem.Bookmark

        store.dispatch(BookmarksListMenuAction.Bookmark.DeleteClicked(bookmarkToDelete))
        val snackState = store.state.bookmarksSnackbarState
        assertTrue(snackState is BookmarksSnackbarState.UndoDeletion && snackState.guidsToDelete.first() == bookmarkToDelete.guid)
        store.dispatch(ViewDisposed)

        verify(bookmarksStorage).deleteNode(bookmarkToDelete.guid)
    }

    @Test
    fun `GIVEN adding a folder WHEN selecting a new parent THEN folder is updated`() = runTestOnMain {
        val tree = generateBookmarkTree()
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(tree)
        val newParent = tree.children?.last { it.type == BookmarkNodeType.FOLDER }!!
        val newParentItem = BookmarkItem.Folder(title = newParent.title!!, guid = newParent.guid)
        val newFolderTitle = "newFolder"
        `when`(bookmarksStorage.addFolder(newParent.guid, newFolderTitle)).thenReturn("new-guid")

        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        store.dispatch(AddFolderClicked)
        store.dispatch(AddFolderAction.TitleChanged(newFolderTitle))
        store.dispatch(AddFolderAction.ParentFolderClicked)
        store.dispatch(SelectFolderAction.ViewAppeared)
        store.dispatch(SelectFolderAction.ItemClicked(SelectFolderItem(0, newParentItem)))
        store.dispatch(BackClicked)

        assertNull(store.state.bookmarksSelectFolderState)
        assertEquals(newParentItem, store.state.bookmarksAddFolderState?.parent)

        store.dispatch(BackClicked)

        verify(bookmarksStorage).addFolder(parentGuid = newParent.guid, title = newFolderTitle)
    }

    @Test
    fun `GIVEN editing a folder WHEN selecting a new parent THEN folder is updated`() = runTestOnMain {
        val tree = generateBookmarkTree()
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(tree)
        val folder = tree.children?.first { it.type == BookmarkNodeType.FOLDER }!!
        val newParent = tree.children?.last { it.type == BookmarkNodeType.FOLDER }!!
        val folderItem = BookmarkItem.Folder(title = folder.title!!, guid = folder.guid)
        val newParentItem = BookmarkItem.Folder(title = newParent.title!!, guid = newParent.guid)

        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        store.dispatch(BookmarksListMenuAction.Folder.EditClicked(folderItem))
        store.dispatch(EditFolderAction.ParentFolderClicked)
        store.dispatch(SelectFolderAction.ViewAppeared)
        store.dispatch(SelectFolderAction.ItemClicked(SelectFolderItem(0, newParentItem)))
        store.dispatch(BackClicked)

        assertNull(store.state.bookmarksSelectFolderState)
        assertEquals(newParentItem, store.state.bookmarksEditFolderState?.parent)

        store.dispatch(BackClicked)

        verify(bookmarksStorage).updateNode(
            folder.guid,
            BookmarkInfo(
                parentGuid = newParent.guid,
                position = tree.children?.indexOfFirst { it.guid == folder.guid }!!.toUInt(),
                title = folder.title,
                url = null,
            ),
        )
    }

    @Test
    fun `GIVEN editing a bookmark WHEN selecting a new parent THEN user can successfully add a new folder`() = runTestOnMain {
        val tree = generateBookmarkTree()
        `when`(bookmarksStorage.countBookmarksInTrees(listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id))).thenReturn(0u)
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(tree)
        `when`(bookmarksStorage.addFolder("folder guid 4", "newFolder")).thenReturn("new-guid")
        val bookmark = tree.children?.first { it.type == BookmarkNodeType.ITEM }!!
        val bookmarkItem = BookmarkItem.Bookmark(title = bookmark.title!!, guid = bookmark.guid, url = bookmark.url!!, previewImageUrl = bookmark.url!!)
        val newFolderTitle = "newFolder"
        val newFolderGuid = "newFolderGuid"
        val parentForNewFolder = tree.children?.last { it.type == BookmarkNodeType.FOLDER }!!
        val parentForNewFolderItem = BookmarkItem.Folder(title = parentForNewFolder.title!!, guid = parentForNewFolder.guid)

        val middleware = buildMiddleware()
        val store = middleware.makeStore()

        store.dispatch(BookmarksListMenuAction.Bookmark.EditClicked(bookmarkItem))
        store.dispatch(EditFolderAction.ParentFolderClicked)
        store.dispatch(SelectFolderAction.ViewAppeared)
        store.dispatch(AddFolderClicked)
        store.dispatch(AddFolderAction.TitleChanged(newFolderTitle))
        store.dispatch(AddFolderAction.ParentFolderClicked)
        store.dispatch(SelectFolderAction.ItemClicked(SelectFolderItem(0, parentForNewFolderItem)))
        store.dispatch(BackClicked)

        assertNotNull(store.state.bookmarksSelectFolderState)
        assertNull(store.state.bookmarksSelectFolderState?.innerSelectionGuid)
        assertEquals(parentForNewFolderItem, store.state.bookmarksAddFolderState?.parent)
        assertEquals(newFolderTitle, store.state.bookmarksAddFolderState?.folderBeingAddedTitle)

        store.dispatch(BackClicked)
        assertNull(store.state.bookmarksAddFolderState)
        verify(bookmarksStorage).addFolder(parentGuid = parentForNewFolder.guid, title = newFolderTitle)

        // replace the previous parent for the new folder in the tree with the updated version
        val newFolder = generateBookmarkFolder(guid = newFolderGuid, title = newFolderTitle, parentForNewFolder.guid)
        val updatedParentForNewFolder = parentForNewFolder.copy(
            children = listOf(newFolder),
        )
        val updatedTree = tree.copy(
            children = tree.children!!.mapNotNull { it.takeIf { it.guid != parentForNewFolder.guid } } + updatedParentForNewFolder,
        )
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id, recursive = true)).thenReturn(updatedTree)
        store.dispatch(SelectFolderAction.ViewAppeared)

        val selectFolderItem = store.state.bookmarksSelectFolderState?.folders?.find { it.guid == newFolderGuid }!!
        store.dispatch(SelectFolderAction.ItemClicked(selectFolderItem))
        store.dispatch(BackClicked)

        assertNull(store.state.bookmarksSelectFolderState)
        assertEquals(newFolderGuid, store.state.bookmarksEditBookmarkState?.folder?.guid)
        assertEquals(newFolderTitle, store.state.bookmarksEditBookmarkState?.folder?.title)
    }

    private fun buildMiddleware() = BookmarksMiddleware(
        bookmarksStorage = bookmarksStorage,
        clipboardManager = clipboardManager,
        addNewTabUseCase = addNewTabUseCase,
        getNavController = { navController },
        exitBookmarks = exitBookmarks,
        wasPreviousAppDestinationHome = wasPreviousAppDestinationHome,
        navigateToSearch = navigateToSearch,
        navigateToSignIntoSync = navigateToSignIntoSync,
        shareBookmark = shareBookmark,
        showTabsTray = showTabsTray,
        resolveFolderTitle = resolveFolderTitle,
        showUrlCopiedSnackbar = showUrlCopiedSnackbar,
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

    private fun generateDesktopRootTree() = BookmarkNode(
        type = BookmarkNodeType.FOLDER,
        guid = BookmarkRoot.Root.id,
        parentGuid = null,
        position = 0U,
        title = "root",
        url = null,
        dateAdded = 0,
        lastModified = 0,
        children = listOf(
            generateBookmarkFolder(BookmarkRoot.Menu.id, "Menu", BookmarkRoot.Root.id),
            generateBookmarkFolder(BookmarkRoot.Toolbar.id, "Toolbar", BookmarkRoot.Root.id),
            generateBookmarkFolder(BookmarkRoot.Unfiled.id, "Unfiled", BookmarkRoot.Root.id),
            generateBookmarkTree(),
        ),
    )

    private fun generateBookmarkTree() = BookmarkNode(
        type = BookmarkNodeType.FOLDER,
        guid = BookmarkRoot.Mobile.id,
        parentGuid = null,
        position = 0U,
        title = "mobile",
        url = null,
        dateAdded = 0,
        lastModified = 0,
        children = generateBookmarkFolders(BookmarkRoot.Mobile.id) + bookmarkItems,
    )

    private fun generateBookmarkFolder(guid: String, title: String, parentGuid: String) = BookmarkNode(
        type = BookmarkNodeType.FOLDER,
        guid = guid,
        parentGuid = parentGuid,
        position = 0U,
        title = title,
        url = null,
        dateAdded = 0,
        lastModified = 0,
        children = bookmarkItems,
    )

    private fun generateBookmark(guid: String, title: String, url: String, lastModified: Long = 0) = BookmarkNode(
        type = BookmarkNodeType.ITEM,
        guid = guid,
        parentGuid = null,
        position = 0U,
        title = title,
        url = url,
        dateAdded = 0,
        lastModified = lastModified,
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
