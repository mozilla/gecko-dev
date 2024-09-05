/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class BookmarksReducerKtTest {
    @Test
    fun `WHEN store initializes THEN no changes to state`() {
        val state = BookmarksState.default

        assertEquals(state, bookmarksReducer(state, Init))
    }

    @Test
    fun `WHEN bookmarks are loaded THEN they are added to state and folder title is updated`() {
        val state = BookmarksState.default
        val items = List(5) {
            BookmarkItem.Folder("$it", "guid$it")
        }
        val newTitle = "bookmarks"

        val result = bookmarksReducer(state, BookmarksLoaded(folderTitle = newTitle, bookmarkItems = items))

        assertEquals(state.copy(folderTitle = newTitle, bookmarkItems = items), result)
    }

    @Test
    fun `GIVEN a bookmark is not selected WHEN long clicking a bookmark THEN it is added to selected items`() {
        val items = listOf(generateBookmark())
        val state = BookmarksState.default.copy(bookmarkItems = items)

        val result = bookmarksReducer(state, BookmarkLongClicked(items[0]))

        assertEquals(items[0], result.selectedItems[0])
    }

    @Test
    fun `GIVEN a bookmark is already selected WHEN long clicking a bookmark THEN it is added to selected items`() {
        val items = listOf(generateBookmark())
        val state = BookmarksState.default.copy(bookmarkItems = items, selectedItems = items)

        val result = bookmarksReducer(state, BookmarkLongClicked(items[0]))

        assertTrue(result.selectedItems.isEmpty())
    }

    @Test
    fun `GIVEN a folder is not selected WHEN long clicking a folder THEN it is added to selected items`() {
        val items = listOf(generateFolder())
        val state = BookmarksState.default.copy(bookmarkItems = items)

        val result = bookmarksReducer(state, FolderLongClicked(items[0]))

        assertEquals(items[0], result.selectedItems[0])
    }

    @Test
    fun `GIVEN a folder is already selected WHEN long clicking a folder THEN it is added to selected items`() {
        val items = listOf(generateFolder())
        val state = BookmarksState.default.copy(bookmarkItems = items, selectedItems = items)

        val result = bookmarksReducer(state, FolderLongClicked(items[0]))

        assertTrue(result.selectedItems.isEmpty())
    }

    @Test
    fun `GIVEN there are already selected items WHEN clicking an unselected bookmark THEN it is added to selected items`() {
        val bookmark1 = generateBookmark(1)
        val bookmark2 = generateBookmark(2)
        val items = listOf(bookmark1, bookmark2)
        val selectedItems = listOf(bookmark1)
        val state = BookmarksState.default.copy(bookmarkItems = items, selectedItems = selectedItems)

        val result = bookmarksReducer(state, BookmarkClicked(bookmark2))

        assertTrue(result.selectedItems.contains(bookmark2))
    }

    @Test
    fun `GIVEN there are already selected items WHEN clicking an unselected folder THEN it is added to selected items`() {
        val folder1 = generateFolder(1)
        val folder2 = generateFolder(2)
        val items = listOf(folder1, folder2)
        val selectedItems = listOf(folder1)
        val state = BookmarksState.default.copy(bookmarkItems = items, selectedItems = selectedItems)

        val result = bookmarksReducer(state, FolderClicked(folder2))

        assertTrue(result.selectedItems.contains(folder2))
    }

    @Test
    fun `GIVEN there are already selected items WHEN clicking a selected bookmark THEN it is removed from selected items`() {
        val bookmark1 = generateBookmark(1)
        val bookmark2 = generateBookmark(2)
        val items = listOf(bookmark1, bookmark2)
        val selectedItems = listOf(bookmark1, bookmark2)
        val state = BookmarksState.default.copy(bookmarkItems = items, selectedItems = selectedItems)

        val result = bookmarksReducer(state, BookmarkClicked(bookmark2))

        assertFalse(result.selectedItems.contains(bookmark2))
    }

    @Test
    fun `GIVEN there are already selected items WHEN clicking a selected folder THEN it is removed from selected items`() {
        val folder1 = generateFolder(1)
        val folder2 = generateFolder(2)
        val items = listOf(folder1, folder2)
        val selectedItems = listOf(folder1, folder2)
        val state = BookmarksState.default.copy(bookmarkItems = items, selectedItems = selectedItems)

        val result = bookmarksReducer(state, FolderClicked(folder2))

        assertFalse(result.selectedItems.contains(folder2))
    }

    private fun generateBookmark(
        num: Int = 0,
        url: String = "url",
        title: String = "title",
        previewImageUrl: String = "previewImageUrl",
    ) = BookmarkItem.Bookmark(url, title, previewImageUrl, "$num")

    private fun generateFolder(
        num: Int = 0,
        title: String = "title",
    ) = BookmarkItem.Folder(title, "$num")
}
