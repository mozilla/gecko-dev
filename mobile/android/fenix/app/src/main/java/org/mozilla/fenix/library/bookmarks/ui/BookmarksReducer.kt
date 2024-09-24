/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

/**
 * Function for reducing a new bookmarks state based on the received action.
 */
internal fun bookmarksReducer(state: BookmarksState, action: BookmarksAction) = when (action) {
    is BookmarksLoaded -> state.copy(
        currentFolder = action.folder,
        bookmarkItems = action.bookmarkItems,
    )
    is BookmarkLongClicked -> state.toggleSelectionOf(action.item)
    is FolderLongClicked -> state.toggleSelectionOf(action.item)
    is FolderClicked -> if (state.selectedItems.isNotEmpty()) {
        state.toggleSelectionOf(action.item)
    } else {
        state
    }
    is EditBookmarkClicked -> state.copy(
        bookmarksEditBookmarkState = BookmarksEditBookmarkState(
            bookmark = action.bookmark,
            folder = BookmarkItem.Folder(title = state.currentFolder.title, guid = state.currentFolder.guid),
        ),
    )
    is BookmarkClicked -> if (state.selectedItems.isNotEmpty()) {
        state.toggleSelectionOf(action.item)
    } else {
        state
    }
    is AddFolderAction.TitleChanged -> state.copy(
        bookmarksAddFolderState = BookmarksAddFolderState(
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
    EditBookmarkAction.DeleteClicked -> state.copy(bookmarksEditBookmarkState = null)
    BackClicked -> state.respondToBackClick()
    EditBookmarkAction.FolderClicked,
    AddFolderAction.ParentFolderClicked,
    SearchClicked,
    AddFolderClicked,
    SignIntoSyncClicked,
    Init,
    -> state
}

private fun BookmarksState.toggleSelectionOf(item: BookmarkItem): BookmarksState =
    if (selectedItems.any { it.guid == item.guid }) {
        copy(selectedItems = selectedItems - item)
    } else {
        copy(selectedItems = selectedItems + item)
    }

private fun BookmarksState.respondToBackClick(): BookmarksState = when {
    bookmarksAddFolderState != null -> copy(bookmarksAddFolderState = null)
    bookmarksEditBookmarkState != null -> copy(bookmarksEditBookmarkState = null)
    else -> this
}
