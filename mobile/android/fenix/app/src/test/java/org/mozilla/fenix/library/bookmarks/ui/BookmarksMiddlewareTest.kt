/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.concept.storage.BookmarksStorage
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.mockito.Mockito.`when`

@RunWith(AndroidJUnit4::class)
class BookmarksMiddlewareTest {

    @get:Rule
    val coroutineRule = MainCoroutineRule()

    private val bookmarksStorage: BookmarksStorage = mock()

    @Test
    fun `GIVEN bookmarks in storage WHEN store is initialized THEN bookmarks will be loaded as display format`() = runTestOnMain {
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(generateBookmarkTree())
        val middleware = BookmarksMiddleware(bookmarksStorage = bookmarksStorage, scope = this)

        val store = middleware.makeStore()

        assertEquals(10, store.state.bookmarkItems.size)
    }

    @Test
    fun `GIVEN no bookmarks under mobile root WHEN store is initialized THEN list of bookmarks will be empty`() = runTestOnMain {
        `when`(bookmarksStorage.getTree(BookmarkRoot.Mobile.id)).thenReturn(null)
        val middleware = BookmarksMiddleware(bookmarksStorage = bookmarksStorage, scope = this)

        val store = middleware.makeStore(
            initialState = BookmarksState(bookmarkItems = listOf(BookmarkItem.Folder("folder"))),
        )

        assertEquals(0, store.state.bookmarkItems.size)
    }

    private fun BookmarksMiddleware.makeStore(
        initialState: BookmarksState = BookmarksState.default,
    ) = BookmarksStore(
        initialState = initialState,
        middleware = listOf(this),
    )

    private fun generateBookmarkTree() = BookmarkNode(
        type = BookmarkNodeType.FOLDER,
        guid = BookmarkRoot.Mobile.id,
        parentGuid = null,
        position = 0U,
        title = "mobile",
        url = null,
        dateAdded = 0L,
        children = List(5) {
            generateBookmarkFolder("folder guid $it", "folder title $it")
        } + List(5) {
            generateBookmark("item guid $it", "item title $it", "item url $it")
        },
    )

    private fun generateBookmarkFolder(guid: String, title: String) = BookmarkNode(
        type = BookmarkNodeType.FOLDER,
        guid = guid,
        parentGuid = null,
        position = 0U,
        title = title,
        url = null,
        dateAdded = 0L,
        children = listOf(),
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
}
