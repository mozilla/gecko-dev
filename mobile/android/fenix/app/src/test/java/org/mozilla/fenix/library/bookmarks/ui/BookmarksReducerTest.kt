/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

import mozilla.appservices.places.BookmarkRoot
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class BookmarksReducerTest {
    @Test
    fun `WHEN store initializes THEN no changes to state`() {
        val state = BookmarksState.default

        assertEquals(state, bookmarksReducer(state, Init))
    }

    @Test
    fun `WHEN bookmarks are loaded THEN they are added to state with their parent folder data`() {
        val state = BookmarksState.default
        val items = List(5) {
            BookmarkItem.Folder("$it", "guid$it")
        }
        val newFolder = BookmarkItem.Folder(
            guid = "guid",
            title = "Bookmarks",
        )

        val result = bookmarksReducer(
            state,
            BookmarksLoaded(
                folder = newFolder,
                bookmarkItems = items,
            ),
        )

        val expected = state.copy(
            currentFolder = newFolder,
            bookmarkItems = items,
        )
        assertEquals(expected, result)
    }

    @Test
    fun `WHEN a user clicks add a folder THEN initialize add folder sub screen state`() {
        val state = BookmarksState.default.copy(
            currentFolder = BookmarkItem.Folder(
                guid = "guid",
                title = "mozilla",
            ),
        )

        val result = bookmarksReducer(state, AddFolderClicked).bookmarksAddFolderState
        val expected = BookmarksAddFolderState(
            parent = BookmarkItem.Folder(
                guid = "guid",
                title = "mozilla",
            ),
            folderBeingAddedTitle = "",
        )

        assertEquals(expected, result)
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

    @Test
    fun `WHEN the title of a folder is changed on the add folder screen THEN that is reflected in state`() {
        val state = BookmarksState.default.copy(
            bookmarksAddFolderState = BookmarksAddFolderState(
                parent = BookmarkItem.Folder(
                    guid = BookmarkRoot.Mobile.id,
                    title = "Bookmarks",
                ),
                folderBeingAddedTitle = "",
            ),
        )

        val titleChange = "test"

        val result = bookmarksReducer(state, AddFolderAction.TitleChanged(titleChange))

        assertEquals(titleChange, result.bookmarksAddFolderState?.folderBeingAddedTitle)
    }

    @Test
    fun `GIVEN we are on the add folder screen WHEN back is clicked THEN add folder state is removed`() {
        val state = BookmarksState.default.copy(
            bookmarksAddFolderState = BookmarksAddFolderState(
                parent = BookmarkItem.Folder(
                    guid = BookmarkRoot.Mobile.id,
                    title = "Bookmarks",
                ),
                folderBeingAddedTitle = "",
            ),
        )

        val result = bookmarksReducer(state, BackClicked)

        assertEquals(BookmarksState.default, result)
    }

    @Test
    fun `GIVEN there is no substate screen present WHEN back is clicked THEN state is unchanged`() {
        val state = BookmarksState.default

        val result = bookmarksReducer(state, BackClicked)

        assertEquals(BookmarksState.default, result)
    }

    @Test
    fun `GIVEN we are on the select folder screen WHEN folders are loaded THEN attach loaded folders on the select screen state`() {
        val state = BookmarksState.default.copy(
            bookmarksSelectFolderState = BookmarksSelectFolderState(),
        )

        val folders = listOf(
            SelectFolderItem(0, BookmarkItem.Folder("Bookmarks", "guid0")),
            SelectFolderItem(1, BookmarkItem.Folder("Nested One", "guid0")),
            SelectFolderItem(2, BookmarkItem.Folder("Nested Two", "guid0")),
            SelectFolderItem(2, BookmarkItem.Folder("Nested Two", "guid0")),
            SelectFolderItem(1, BookmarkItem.Folder("Nested One", "guid0")),
            SelectFolderItem(2, BookmarkItem.Folder("Nested Two", "guid1")),
            SelectFolderItem(3, BookmarkItem.Folder("Nested Three", "guid0")),
            SelectFolderItem(0, BookmarkItem.Folder("Nested 0", "guid0")),
        )

        val result = bookmarksReducer(state, SelectFolderAction.FoldersLoaded(folders))

        val expected = state.copy(
            bookmarksSelectFolderState = BookmarksSelectFolderState(folders = folders),
        )

        assertEquals(expected, result)
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
