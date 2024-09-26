/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

/**
 * Function for reducing a new bookmarks state based on the received action.
 */
@Suppress("LongMethod")
internal fun bookmarksReducer(state: BookmarksState, action: BookmarksAction) = when (action) {
    is BookmarksLoaded -> state.copy(
        currentFolder = action.folder,
        bookmarkItems = action.bookmarkItems,
    )
    is BookmarkLongClicked -> state.toggleSelectionOf(action.item)
    is FolderLongClicked -> state.toggleSelectionOf(action.item)
    is FolderClicked -> if (state.selectedItems.isNotEmpty() && !action.item.isDesktopFolder) {
        state.toggleSelectionOf(action.item)
    } else {
        state
    }
    is EditBookmarkClicked -> state.copy(
        bookmarksEditBookmarkState = BookmarksEditBookmarkState(
            bookmark = action.bookmark,
            folder = state.currentFolder,
        ),
    )
    is BookmarkClicked -> if (state.selectedItems.isNotEmpty()) {
        state.toggleSelectionOf(action.item)
    } else {
        state
    }
    is AddFolderAction.TitleChanged -> state.copy(
        bookmarksAddFolderState = state.bookmarksAddFolderState?.copy(
            folderBeingAddedTitle = action.updatedText,
        ),
    )
    is EditBookmarkAction.TitleChanged -> state.copy(
        bookmarksEditBookmarkState = state.bookmarksEditBookmarkState?.let {
            it.copy(
                bookmark = it.bookmark.copy(title = action.title),
            )
        },
    )
    is EditBookmarkAction.URLChanged -> state.copy(
        bookmarksEditBookmarkState = state.bookmarksEditBookmarkState?.let {
            it.copy(
                bookmark = it.bookmark.copy(url = action.url),
            )
        },
    )
    is SelectFolderAction.FoldersLoaded -> state.copy(
        bookmarksSelectFolderState = state.bookmarksSelectFolderState?.copy(
            folders = action.folders,
        ),
    )
    AddFolderClicked -> state.copy(
        bookmarksAddFolderState = BookmarksAddFolderState(
            parent = state.currentFolder,
            folderBeingAddedTitle = "",
        ),
    )
    is SelectFolderAction.ItemClicked -> state.updateSelectedFolder(action.folder)
    EditBookmarkAction.DeleteClicked -> state.copy(bookmarksEditBookmarkState = null)
    BackClicked -> state.respondToBackClick()
    EditBookmarkAction.FolderClicked -> state.copy(
        bookmarksSelectFolderState = BookmarksSelectFolderState(
            selectionGuid = state.bookmarksEditBookmarkState?.folder?.guid ?: state.currentFolder.guid,
        ),
    )
    AddFolderAction.ParentFolderClicked -> state.copy(
        bookmarksSelectFolderState = BookmarksSelectFolderState(
            folderSelectionGuid = state.bookmarksAddFolderState?.parent?.guid ?: state.currentFolder.guid,
        ),
    )
    is EditFolderAction.TitleChanged -> state.copy(
        bookmarksEditFolderState = state.bookmarksEditFolderState?.let {
            it.copy(
                folder = it.folder.copy(title = action.updatedText),
            )
        },
    )
    EditFolderAction.ParentFolderClicked -> state.copy(
        bookmarksSelectFolderState = BookmarksSelectFolderState(
            folderSelectionGuid = state.bookmarksEditFolderState?.parent?.guid ?: state.currentFolder.guid,
        ),
    )
    is BookmarksListMenuAction -> state.handleListMenuAction(action)
    SelectFolderAction.ViewAppeared,
    SearchClicked,
    SignIntoSyncClicked,
    Init,
    -> state
}

private fun BookmarksState.updateSelectedFolder(folder: SelectFolderItem): BookmarksState = when {
    bookmarksSelectFolderState?.folderSelectionGuid != null -> {
        // we can't have both an add and edit folder at the same time, so we will just try to update
        // both of them.
        copy(
            bookmarksEditFolderState = bookmarksEditFolderState?.copy(parent = folder.folder),
            bookmarksAddFolderState = bookmarksAddFolderState?.copy(parent = folder.folder),
            bookmarksSelectFolderState = bookmarksSelectFolderState.copy(folderSelectionGuid = folder.guid),
        )
    }
    bookmarksSelectFolderState?.selectionGuid != null -> {
        copy(
            bookmarksEditBookmarkState = bookmarksEditBookmarkState?.copy(folder = folder.folder),
            bookmarksSelectFolderState = bookmarksSelectFolderState.copy(selectionGuid = folder.guid),
        )
    }
    else -> this
}

private fun BookmarksState.toggleSelectionOf(item: BookmarkItem): BookmarksState =
    if (selectedItems.any { it.guid == item.guid }) {
        copy(selectedItems = selectedItems - item)
    } else {
        copy(selectedItems = selectedItems + item)
    }

private fun BookmarksSelectFolderState.respondToBackClick(): BookmarksSelectFolderState? = when {
    selectionGuid != null && folderSelectionGuid != null -> copy(folderSelectionGuid = null)
    else -> null
}

private fun BookmarksState.respondToBackClick(): BookmarksState = when {
    bookmarksSelectFolderState != null -> copy(
        bookmarksSelectFolderState = bookmarksSelectFolderState.respondToBackClick(),
    )
    bookmarksAddFolderState != null -> copy(bookmarksAddFolderState = null)
    bookmarksEditFolderState != null -> copy(bookmarksEditFolderState = null)
    bookmarksEditBookmarkState != null -> copy(bookmarksEditBookmarkState = null)
    else -> this
}

private fun BookmarksState.handleListMenuAction(action: BookmarksListMenuAction): BookmarksState =
    when (action) {
        is BookmarksListMenuAction.Bookmark.EditClicked -> this.copy(
            bookmarksEditBookmarkState = BookmarksEditBookmarkState(
                bookmark = action.bookmark,
                folder = currentFolder,
            ),
        )
        is BookmarksListMenuAction.Folder.EditClicked -> copy(
            bookmarksEditFolderState = BookmarksEditFolderState(
                parent = currentFolder,
                folder = action.folder,
            ),
        )
        is BookmarksListMenuAction.MultiSelect.EditClicked ->
            selectedItems.firstOrNull()?.let { selectedItem ->
                if (selectedItem is BookmarkItem.Bookmark) {
                    copy(
                        bookmarksEditBookmarkState = BookmarksEditBookmarkState(
                            bookmark = selectedItem,
                            folder = currentFolder,
                        ),
                    )
                } else {
                    this // TODO
                }
            } ?: this
        else -> this
    }.let { updatedState ->
        if (action is BookmarksListMenuAction.MultiSelect) {
            updatedState.copy(selectedItems = listOf())
        } else {
            updatedState
        }
    }
