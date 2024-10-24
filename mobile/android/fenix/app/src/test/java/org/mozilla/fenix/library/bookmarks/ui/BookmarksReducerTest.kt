/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

import mozilla.appservices.places.BookmarkRoot
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
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
    fun `WHEN a folder is created THEN remove the select folder state and update the edit bookmark state`() {
        val state = BookmarksState.default.copy(
            bookmarksSelectFolderState = BookmarksSelectFolderState(outerSelectionGuid = "guid"),
            bookmarksEditBookmarkState = BookmarksEditBookmarkState(
                bookmark = BookmarkItem.Bookmark("url", "title", "url", "guid"),
                folder = BookmarkItem.Folder("parentTitle", "parentGuid"),
            ),
        )

        val folder = BookmarkItem.Folder("New Bookmark", "guid")
        val result = bookmarksReducer(state, AddFolderAction.FolderCreated(folder))

        assertEquals(folder, result.bookmarksEditBookmarkState?.folder)
        assertNull(result.bookmarksSelectFolderState)
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
    fun `GIVEN we are on the add folder screen WHEN parent folder is clicked THEN initialize the select folder state`() {
        val state = BookmarksState.default.copy(
            bookmarksAddFolderState = BookmarksAddFolderState(
                parent = BookmarkItem.Folder(
                    guid = BookmarkRoot.Mobile.id,
                    title = "Bookmarks",
                ),
                folderBeingAddedTitle = "",
            ),
        )

        val result = bookmarksReducer(state, AddFolderAction.ParentFolderClicked)

        assertEquals(BookmarkRoot.Mobile.id, result.bookmarksSelectFolderState?.outerSelectionGuid)
    }

    @Test
    fun `GIVEN the add folder screen has been reached from the select folder screen WHEN back clicked THEN only inner selection is removed`() {
        val state = BookmarksState.default.copy(
            bookmarksEditBookmarkState = BookmarksEditBookmarkState(
                bookmark = BookmarkItem.Bookmark("url", "title", "url", "guid"),
                folder = BookmarkItem.Folder("parentTitle", "parentGuid"),
            ),
            bookmarksAddFolderState = BookmarksAddFolderState(
                parent = BookmarkItem.Folder(
                    guid = BookmarkRoot.Mobile.id,
                    title = "Bookmarks",
                ),
                folderBeingAddedTitle = "",
            ),
            bookmarksSelectFolderState = BookmarksSelectFolderState(
                outerSelectionGuid = "outerGuid",
                innerSelectionGuid = "innerGuid",
            ),
        )

        val result = bookmarksReducer(state, BackClicked)

        assertNull(result.bookmarksSelectFolderState?.innerSelectionGuid)
        assertEquals("outerGuid", result.bookmarksSelectFolderState?.outerSelectionGuid)
    }

    @Test
    fun `WHEN the title of a folder is changed on the edit folder screen THEN that is reflected in state`() {
        val state = BookmarksState.default.copy(
            bookmarksEditFolderState = BookmarksEditFolderState(
                parent = BookmarkItem.Folder(
                    guid = BookmarkRoot.Mobile.id,
                    title = "Bookmarks",
                ),
                folder = BookmarkItem.Folder(
                    guid = "guid1",
                    title = "New Bookmarks",
                ),
            ),
        )

        val titleChange = "New Bookmarks 2"

        val result = bookmarksReducer(state, EditFolderAction.TitleChanged(titleChange))

        assertEquals(titleChange, result.bookmarksEditFolderState?.folder?.title)
    }

    @Test
    fun `GIVEN we are on the edit folder screen WHEN back is clicked THEN add folder state is removed`() {
        val state = BookmarksState.default.copy(
            bookmarksEditFolderState = BookmarksEditFolderState(
                parent = BookmarkItem.Folder(
                    guid = BookmarkRoot.Mobile.id,
                    title = "Bookmarks",
                ),
                folder = BookmarkItem.Folder(
                    guid = "guid1",
                    title = "New Bookmarks",
                ),
            ),
        )

        val result = bookmarksReducer(state, BackClicked)

        assertEquals(BookmarksState.default, result)
    }

    @Test
    fun `GIVEN we are on the edit folder screen WHEN parent folder is clicked THEN initialize the select folder state`() {
        val state = BookmarksState.default.copy(
            bookmarksEditFolderState = BookmarksEditFolderState(
                parent = BookmarkItem.Folder(
                    guid = BookmarkRoot.Mobile.id,
                    title = "Bookmarks",
                ),
                folder = BookmarkItem.Folder(
                    guid = "guid1",
                    title = "New Bookmarks",
                ),
            ),
        )

        val result = bookmarksReducer(state, EditFolderAction.ParentFolderClicked)

        assertEquals(BookmarkRoot.Mobile.id, result.bookmarksSelectFolderState?.outerSelectionGuid)
    }

    @Test
    fun `GIVEN we are on the edit bookmark screen WHEN folder is clicked THEN initialize the select folder state`() {
        val state = BookmarksState.default.copy(
            bookmarksEditBookmarkState = BookmarksEditBookmarkState(
                bookmark = BookmarkItem.Bookmark("", "", "", ""),
                folder = BookmarkItem.Folder(
                    guid = "1",
                    title = "Bookmarks",
                ),
            ),
        )

        val result = bookmarksReducer(state, EditBookmarkAction.FolderClicked)

        assertEquals("1", result.bookmarksSelectFolderState?.outerSelectionGuid)
    }

    @Test
    fun `GIVEN there is no substate screen present WHEN back is clicked THEN state is unchanged`() {
        val state = BookmarksState.default

        val result = bookmarksReducer(state, BackClicked)

        assertEquals(BookmarksState.default, result)
    }

    @Test
    fun `GIVEN we are on the select folder screen WHEN back is clicked THEN the select folder state is removed`() {
        val state = BookmarksState.default.copy(
            bookmarksSelectFolderState = BookmarksSelectFolderState(outerSelectionGuid = "guid"),
        )

        val result = bookmarksReducer(state, BackClicked)

        val expected = state.copy(
            bookmarksSelectFolderState = null,
        )

        assertEquals(expected, result)
    }

    @Test
    fun `GIVEN we are on the select folder screen with multi-select state WHEN back is clicked THEN the select folder and multi-select state is removed`() {
        val state = BookmarksState.default.copy(
            bookmarksMultiselectMoveState = MultiselectMoveState(
                guidsToMove = listOf(),
                destination = "",
            ),
            bookmarksSelectFolderState = BookmarksSelectFolderState(outerSelectionGuid = ""),
        )

        val result = bookmarksReducer(state, BackClicked)

        val expected = state.copy(
            bookmarksMultiselectMoveState = null,
            bookmarksSelectFolderState = null,
        )

        assertEquals(expected, result)
    }

    @Test
    fun `GIVEN we are on the select folder screen while editing a bookmark and creating a folder WHEN back is clicked THEN just the select folder guid is removed`() {
        val state = BookmarksState.default.copy(
            bookmarksSelectFolderState = BookmarksSelectFolderState(
                outerSelectionGuid = "abc",
                innerSelectionGuid = "123",
            ),
        )

        val result = bookmarksReducer(state, BackClicked)

        val expected = state.copy(
            bookmarksSelectFolderState = state.bookmarksSelectFolderState?.copy(
                innerSelectionGuid = null,
            ),
        )

        assertEquals(expected, result)
    }

    @Test
    fun `GIVEN we are on the select folder screen WHEN folders are loaded THEN attach loaded folders on the select screen state`() {
        val state = BookmarksState.default.copy(
            bookmarksSelectFolderState = BookmarksSelectFolderState(outerSelectionGuid = "selection guid"),
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
            bookmarksSelectFolderState = BookmarksSelectFolderState(
                outerSelectionGuid = "selection guid",
                folders = folders,
            ),
        )

        assertEquals(expected, result)
    }

    @Test
    fun `GIVEN we are on the select folder screen with a selection guid WHEN a folder is clicked THEN update the selection`() {
        val state = BookmarksState.default.copy(
            bookmarksEditBookmarkState = BookmarksEditBookmarkState(
                bookmark = generateBookmark(),
                folder = BookmarkItem.Folder("Bookmarks", "guid0"),
            ),
            bookmarksSelectFolderState = BookmarksSelectFolderState(
                outerSelectionGuid = "guid0",
                folders = listOf(
                    SelectFolderItem(0, BookmarkItem.Folder("Bookmarks", "guid0")),
                    SelectFolderItem(0, BookmarkItem.Folder("Nested 0", "guid0")),
                ),
            ),
        )

        val result = bookmarksReducer(
            state = state,
            action = SelectFolderAction.ItemClicked(
                folder = SelectFolderItem(0, BookmarkItem.Folder("Nested 0", "guid0")),
            ),
        )

        val expected = state.copy(
            bookmarksEditBookmarkState = state.bookmarksEditBookmarkState?.copy(
                folder = BookmarkItem.Folder("Nested 0", "guid0"),
            ),
            bookmarksSelectFolderState = state.bookmarksSelectFolderState?.copy(
                outerSelectionGuid = "guid0",
            ),
        )

        assertEquals(expected, result)
    }

    @Test
    fun `GIVEN we are on the select folder screen with a selection guid and multiselect state WHEN a folder is clicked THEN update the selection`() {
        val state = BookmarksState.default.copy(
            bookmarksMultiselectMoveState = MultiselectMoveState(
                guidsToMove = listOf("guid0", "guid1"),
                destination = "folder 1",
            ),
            bookmarksSelectFolderState = BookmarksSelectFolderState(
                outerSelectionGuid = "folder 1",
                folders = listOf(
                    SelectFolderItem(0, BookmarkItem.Folder("Bookmarks", "guid 0")),
                    SelectFolderItem(0, BookmarkItem.Folder("Nested 0", "guid 1")),
                    SelectFolderItem(0, BookmarkItem.Folder("Nested 1", "folder 1")),
                    SelectFolderItem(0, BookmarkItem.Folder("Nested 2", "folder 2")),
                ),
            ),
        )

        val result = bookmarksReducer(
            state = state,
            action = SelectFolderAction.ItemClicked(
                folder = SelectFolderItem(0, BookmarkItem.Folder("Nested 2", "folder 2")),
            ),
        )

        val expected = state.copy(
            bookmarksMultiselectMoveState = state.bookmarksMultiselectMoveState?.copy(
                destination = "folder 2",
            ),
            bookmarksSelectFolderState = state.bookmarksSelectFolderState?.copy(
                outerSelectionGuid = "folder 2",
            ),
        )

        assertEquals(expected, result)
    }

    @Test
    fun `GIVEN we are on the select folder screen with a selection and an add folder guid WHEN a folder is clicked THEN update the add folder selection`() {
        val state = BookmarksState.default.copy(
            bookmarksEditBookmarkState = BookmarksEditBookmarkState(
                bookmark = generateBookmark(),
                folder = BookmarkItem.Folder("Bookmarks", "0"),
            ),
            bookmarksAddFolderState = BookmarksAddFolderState(
                parent = BookmarkItem.Folder("Bookmarks", "0"),
                folderBeingAddedTitle = "",
            ),
            bookmarksSelectFolderState = BookmarksSelectFolderState(
                outerSelectionGuid = "0",
                innerSelectionGuid = "0",
                folders = listOf(
                    SelectFolderItem(0, BookmarkItem.Folder("Bookmarks", "0")),
                    SelectFolderItem(0, BookmarkItem.Folder("Nested 0", "1")),
                ),
            ),
        )

        val result = bookmarksReducer(
            state = state,
            action = SelectFolderAction.ItemClicked(
                folder = SelectFolderItem(0, BookmarkItem.Folder("Nested 0", "1")),
            ),
        )

        val expected = state.copy(
            bookmarksAddFolderState = state.bookmarksAddFolderState?.copy(
                parent = BookmarkItem.Folder("Nested 0", "1"),
            ),
            bookmarksSelectFolderState = state.bookmarksSelectFolderState?.copy(
                innerSelectionGuid = "1",
            ),
        )

        assertEquals(expected, result)
    }

    @Test
    fun `WHEN the edit button is clicked in a bookmark menu THEN bookmark edit state is created`() {
        val item = BookmarkItem.Bookmark("ur", "title", "url", "guid")
        val parent = BookmarkItem.Folder("title", "guid")
        val state = BookmarksState.default.copy(
            bookmarkItems = listOf(item),
            currentFolder = parent,
        )

        val result = bookmarksReducer(state, BookmarksListMenuAction.Bookmark.EditClicked(item))
        assertEquals(BookmarksEditBookmarkState(bookmark = item, folder = parent), result.bookmarksEditBookmarkState)
    }

    @Test
    fun `GIVEN one selected item WHEN clicking on a desktop folder THEN display a snackbar explaining that you can't do that`() {
        val item = BookmarkItem.Bookmark("ur", "title", "url", "guid")
        val parent = BookmarkItem.Folder("title", "guid")
        val state = BookmarksState.default.copy(
            bookmarkItems = listOf(item),
            selectedItems = listOf(item),
            currentFolder = parent,
        )

        val folder = BookmarkItem.Folder("Desktop Bookmarks", BookmarkRoot.Root.id)
        val result = bookmarksReducer(state, FolderClicked(folder))
        assertEquals(BookmarksSnackbarState.CantEditDesktopFolders, result.bookmarksSnackbarState)
    }

    @Test
    fun `GIVEN a snackbar is displayed WHEN the snackbar is dismissed THEN update the state`() {
        val state = BookmarksState.default.copy(
            bookmarksSnackbarState = BookmarksSnackbarState.CantEditDesktopFolders,
        )

        val result = bookmarksReducer(state, SnackbarAction.Dismissed)
        assertEquals(BookmarksSnackbarState.None, result.bookmarksSnackbarState)
    }

    @Test
    fun `GIVEN a undo snackbar is displayed WHEN the snackbar is dismissed THEN remove the snackbar and any bookmark items that were deleted`() {
        val state = BookmarksState.default.copy(
            bookmarkItems = listOf(BookmarkItem.Folder("Bookmark Folder", "guid0")),
            bookmarksSnackbarState = BookmarksSnackbarState.UndoDeletion(listOf("guid0")),
        )

        val result = bookmarksReducer(state, SnackbarAction.Dismissed)
        assertEquals(BookmarksSnackbarState.None, result.bookmarksSnackbarState)
        assertEquals(listOf<BookmarkItem>(), result.bookmarkItems)
    }

    @Test
    fun `GIVEN a undo snackbar is displayed WHEN undo is tapped THEN remove the snackbar`() {
        val state = BookmarksState.default.copy(
            bookmarkItems = listOf(BookmarkItem.Folder("Bookmark Folder", "guid0")),
            bookmarksSnackbarState = BookmarksSnackbarState.UndoDeletion(listOf("guid0")),
        )

        val result = bookmarksReducer(state, SnackbarAction.Undo)
        assertEquals(BookmarksSnackbarState.None, result.bookmarksSnackbarState)
        assertEquals(1, result.bookmarkItems.size)
    }

    @Test
    fun `GIVEN a undo snackbar is displayed WHEN another item is deleted THEN append the item to be deleted`() {
        val state = BookmarksState.default.copy(
            bookmarkItems = listOf(BookmarkItem.Folder("Bookmark Folder", "guid0")),
            bookmarksSnackbarState = BookmarksSnackbarState.UndoDeletion(listOf("guid0")),
        )

        val bookmarkToDelete = BookmarkItem.Bookmark(
            guid = "guid1",
            title = "title",
            url = "url",
            previewImageUrl = "previewImage",
        )

        val result = bookmarksReducer(state, BookmarksListMenuAction.Bookmark.DeleteClicked(bookmarkToDelete))
        val expected = BookmarksSnackbarState.UndoDeletion(guidsToDelete = listOf("guid0", "guid1"))
        assertEquals(expected, result.bookmarksSnackbarState)
    }

    @Test
    fun `GIVEN a list of a bookmarks WHEN a folder is deleted THEN load the number of nested bookmarks`() {
        val folder = BookmarkItem.Folder("Bookmark Folder", "guid0")
        val state = BookmarksState.default.copy(
            bookmarkItems = listOf(folder),
        )

        val result = bookmarksReducer(state, BookmarksListMenuAction.Folder.DeleteClicked(folder))
        assertEquals(DeletionDialogState.LoadingCount(listOf("guid0")), result.bookmarksDeletionDialogState)
    }

    @Test
    fun `WHEN folder is deleted from folder edit screen THEN load the number of nested bookmarks`() {
        val folder = BookmarkItem.Folder("Bookmark Folder", "guid0")
        val state = BookmarksState.default.copy(
            bookmarksEditFolderState = BookmarksEditFolderState(folder, folder),
        )

        val result = bookmarksReducer(state, EditFolderAction.DeleteClicked)
        assertEquals(DeletionDialogState.LoadingCount(listOf("guid0")), result.bookmarksDeletionDialogState)
    }

    @Test
    fun `GIVEN a deletion dialog that is loading a count WHEN we receive the count THEN present the dialog`() {
        val folder = BookmarkItem.Folder("Bookmark Folder", "guid0")
        val state = BookmarksState.default.copy(
            bookmarkItems = listOf(folder),
            bookmarksDeletionDialogState = DeletionDialogState.LoadingCount(listOf("guid0")),
        )

        val result = bookmarksReducer(state, DeletionDialogAction.CountLoaded(19))
        assertEquals(DeletionDialogState.Presenting(listOf("guid0"), 19), result.bookmarksDeletionDialogState)
    }

    @Test
    fun `GIVEN a deletion dialog WHEN delete is tapped THEN dismiss the dialog and remove the deleted items`() {
        val folder = BookmarkItem.Folder("Bookmark Folder", "guid0")
        val state = BookmarksState.default.copy(
            bookmarkItems = listOf(folder),
            bookmarksDeletionDialogState = DeletionDialogState.Presenting(listOf("guid0"), 1),
        )

        val result = bookmarksReducer(state, DeletionDialogAction.DeleteTapped)
        assertEquals(DeletionDialogState.None, result.bookmarksDeletionDialogState)
        assertEquals(listOf<BookmarkItem>(), result.bookmarkItems)
    }

    @Test
    fun `GIVEN a deletion dialog WHEN cancel is tapped THEN dismiss the dialog`() {
        val folder = BookmarkItem.Folder("Bookmark Folder", "guid0")
        val state = BookmarksState.default.copy(
            bookmarkItems = listOf(folder),
            bookmarksDeletionDialogState = DeletionDialogState.LoadingCount(listOf("guid0")),
        )

        val result = bookmarksReducer(state, DeletionDialogAction.CancelTapped)
        assertEquals(DeletionDialogState.None, result.bookmarksDeletionDialogState)
        assertEquals(state.bookmarkItems, result.bookmarkItems)
    }

    @Test
    fun `GIVEN one selected item WHEN the edit button is clicked in a bookmark menu THEN bookmark edit state is created`() {
        val item = BookmarkItem.Bookmark("ur", "title", "url", "guid")
        val parent = BookmarkItem.Folder("title", "guid")
        val state = BookmarksState.default.copy(
            bookmarkItems = listOf(item),
            selectedItems = listOf(item),
            currentFolder = parent,
        )

        val result = bookmarksReducer(state, BookmarksListMenuAction.MultiSelect.EditClicked)
        assertEquals(BookmarksEditBookmarkState(bookmark = item, folder = parent), result.bookmarksEditBookmarkState)
    }

    @Test
    fun `GIVEN a single bookmark selected WHEN tapping delete in the multi select menu THEN present the undo snackbar`() {
        val item = BookmarkItem.Bookmark("ur", "title", "url", "guid")
        val state = BookmarksState.default.copy(
            bookmarkItems = listOf(item),
            selectedItems = listOf(item),
        )

        val result = bookmarksReducer(state, BookmarksListMenuAction.MultiSelect.DeleteClicked)
        assertEquals(BookmarksSnackbarState.UndoDeletion(listOf("guid")), result.bookmarksSnackbarState)
    }

    @Test
    fun `GIVEN a single folder selected WHEN tapping delete in the multi select menu THEN present the deletion dialog`() {
        val item = BookmarkItem.Folder("title", "guid")
        val state = BookmarksState.default.copy(
            bookmarkItems = listOf(item),
            selectedItems = listOf(item),
        )

        val result = bookmarksReducer(state, BookmarksListMenuAction.MultiSelect.DeleteClicked)
        assertEquals(DeletionDialogState.LoadingCount(listOf("guid")), result.bookmarksDeletionDialogState)
    }

    @Test
    fun `GIVEN a multiple bookmarks selected WHEN tapping delete in the multi select menu THEN present the deletion dialog`() {
        val item = BookmarkItem.Bookmark("ur", "title", "url", "guid")
        val item2 = item.copy(guid = "guid2")
        val state = BookmarksState.default.copy(
            bookmarkItems = listOf(item, item2),
            selectedItems = listOf(item, item2),
        )

        val result = bookmarksReducer(state, BookmarksListMenuAction.MultiSelect.DeleteClicked)
        assertEquals(DeletionDialogState.LoadingCount(listOf("guid", "guid2")), result.bookmarksDeletionDialogState)
    }

    @Test
    fun `GIVEN a multiple bookmarks selected WHEN receiving a recursive count update THEN update the state`() {
        val item = BookmarkItem.Bookmark("ur", "title", "url", "guid")
        val item2 = item.copy(guid = "guid2")
        val state = BookmarksState.default.copy(
            bookmarkItems = listOf(item, item2),
            selectedItems = listOf(item, item2),
        )

        val result = bookmarksReducer(state, RecursiveSelectionCountLoaded(19))
        assertEquals(state.copy(recursiveSelectedCount = 19), result)
    }

    @Test
    fun `GIVEN an edit bookmark screen WHEN deleting the bookmark THEN present the undo snackbar`() {
        val item = BookmarkItem.Bookmark("ur", "title", "url", "guid")
        val state = BookmarksState.default.copy(
            bookmarksEditBookmarkState = BookmarksEditBookmarkState(
                folder = BookmarkItem.Folder("Bookmarks", BookmarkRoot.Mobile.id),
                bookmark = item,
            ),
        )

        val result = bookmarksReducer(state, EditBookmarkAction.DeleteClicked)
        val expected = state.copy(
            bookmarksEditBookmarkState = null,
            bookmarksSnackbarState = BookmarksSnackbarState.UndoDeletion(listOf(item.guid)),
        )
        assertEquals(expected, result)
    }

    @Test
    fun `GIVEN a user tries to open a folder with more than 15 items WHEN we receive a Present action THEN we show the dialog`() {
        val state = BookmarksState.default
        val action = OpenTabsConfirmationDialogAction.Present(
            guid = "guid0",
            count = 19,
            isPrivate = false,
        )

        val result = bookmarksReducer(state, action)
        val expected = state.copy(
            openTabsConfirmationDialog = OpenTabsConfirmationDialog.Presenting(
                guidToOpen = "guid0",
                numberOfTabs = 19,
                isPrivate = false,
            ),
        )
        assertEquals(expected, result)
    }

    @Test
    fun `GIVEN a user is presented with a open tab confirmation dialog WHEN the user taps cancel or open THEN we close the dialog`() {
        val state = BookmarksState.default.copy(
            openTabsConfirmationDialog = OpenTabsConfirmationDialog.Presenting(
                guidToOpen = "guid0",
                numberOfTabs = 19,
                isPrivate = false,
            ),
        )

        listOf(
            OpenTabsConfirmationDialogAction.CancelTapped,
            OpenTabsConfirmationDialogAction.ConfirmTapped,
        ).forEach {
            assertEquals(BookmarksState.default, bookmarksReducer(state, it))
        }
    }

    @Test
    fun `GIVEN a user is initializing on the edit screen WHEN the data loads THEN update the state`() {
        val state = BookmarksState.default
        val bookmark = BookmarkItem.Bookmark("ur", "title", "url", "guid")
        val parent = BookmarkItem.Folder("title", "guid")

        val result = bookmarksReducer(state, InitEditLoaded(bookmark = bookmark, folder = parent))
        val expected = state.copy(
            currentFolder = parent,
            bookmarksEditBookmarkState = BookmarksEditBookmarkState(
                bookmark = bookmark,
                folder = parent,
            ),
        )
        assertEquals(expected, result)
    }

    @Test
    fun `GIVEN selected items WHEN a multi-select menu action is taken THEN the items are unselected`() {
        val item = BookmarkItem.Bookmark("ur", "title", "url", "guid")
        val parent = BookmarkItem.Folder("title", "guid")
        val state = BookmarksState.default.copy(
            bookmarkItems = listOf(item),
            selectedItems = listOf(item),
            currentFolder = parent,
        )

        var result = bookmarksReducer(state, BookmarksListMenuAction.MultiSelect.EditClicked)
        assertTrue(result.selectedItems.isEmpty())

        result = bookmarksReducer(state, BookmarksListMenuAction.MultiSelect.MoveClicked)
        assertTrue(result.selectedItems.isEmpty())

        result = bookmarksReducer(state, BookmarksListMenuAction.MultiSelect.DeleteClicked)
        assertTrue(result.selectedItems.isEmpty())

        result = bookmarksReducer(state, BookmarksListMenuAction.MultiSelect.OpenInNormalTabsClicked)
        assertTrue(result.selectedItems.isEmpty())

        result = bookmarksReducer(state, BookmarksListMenuAction.MultiSelect.OpenInPrivateTabsClicked)
        assertTrue(result.selectedItems.isEmpty())

        result = bookmarksReducer(state, BookmarksListMenuAction.MultiSelect.ShareClicked)
        assertTrue(result.selectedItems.isEmpty())
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
