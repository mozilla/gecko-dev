/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

import mozilla.appservices.places.BookmarkRoot
import mozilla.components.lib.state.State
import org.mozilla.fenix.R

/**
 * Represents the state of the Bookmarks list screen and its various subscreens.
 *
 * @property bookmarkItems Bookmark items to be displayed in the current list screen.
 * @property selectedItems The bookmark items that are currently selected by the user for bulk actions.
 * @property currentFolder the [BookmarkItem.Folder] that is currently being displayed.
 * @property isSignedIntoSync State representing if the user is currently signed into sync.
 * @property bookmarksSnackbarState State representing which snackbar to show.
 * @property bookmarksAddFolderState State representing the add folder subscreen, if visible.
 * @property bookmarksEditBookmarkState State representing the edit bookmark subscreen, if visible.
 * @property bookmarksSelectFolderState State representing the select folder subscreen, if visible.
 * @property bookmarksEditFolderState State representing the edit folder subscreen, if visible.
 */
internal data class BookmarksState(
    val bookmarkItems: List<BookmarkItem>,
    val selectedItems: List<BookmarkItem>,
    val currentFolder: BookmarkItem.Folder,
    val isSignedIntoSync: Boolean,
    val bookmarksSnackbarState: BookmarksSnackbarState,
    val bookmarksAddFolderState: BookmarksAddFolderState?,
    val bookmarksEditBookmarkState: BookmarksEditBookmarkState?,
    val bookmarksSelectFolderState: BookmarksSelectFolderState?,
    val bookmarksEditFolderState: BookmarksEditFolderState?,
) : State {
    companion object {
        val default: BookmarksState = BookmarksState(
            bookmarkItems = listOf(),
            selectedItems = listOf(),
            currentFolder = BookmarkItem.Folder("", ""),
            isSignedIntoSync = false,
            bookmarksSnackbarState = BookmarksSnackbarState.None,
            bookmarksAddFolderState = null,
            bookmarksEditBookmarkState = null,
            bookmarksSelectFolderState = null,
            bookmarksEditFolderState = null,
        )
    }
}

internal val BookmarkItem.title: String
    get() = when (this) {
        is BookmarkItem.Folder -> this.title
        is BookmarkItem.Bookmark -> this.title
    }

internal fun BookmarksState.undoSnackbarText(): Pair<Int, String> = bookmarksSnackbarState.let { state ->
    when {
        state is BookmarksSnackbarState.UndoDeletion && state.guidsToDelete.size == 1 -> {
            val stringId = R.string.bookmark_delete_single_item
            val title = this.bookmarkItems.first { it.guid == state.guidsToDelete.first() }.title
            stringId to title
        }
        state is BookmarksSnackbarState.UndoDeletion -> {
            val stringId = R.string.bookmark_delete_multiple_items
            val numberOfBookmarks = "${state.guidsToDelete.size}"
            stringId to numberOfBookmarks
        }
        else -> 0 to ""
    }
}

internal fun BookmarksState.isGuidMarkedForDeletion(guid: String): Boolean = when (bookmarksSnackbarState) {
    is BookmarksSnackbarState.UndoDeletion -> bookmarksSnackbarState.guidsToDelete.contains(guid)
    else -> false
}

internal sealed class BookmarksSnackbarState {
    data object None : BookmarksSnackbarState()
    data object CantEditDesktopFolders : BookmarksSnackbarState()
    data class UndoDeletion(val guidsToDelete: List<String>) : BookmarksSnackbarState()
}

internal data class BookmarksEditBookmarkState(
    val bookmark: BookmarkItem.Bookmark,
    val folder: BookmarkItem.Folder,
)

internal data class BookmarksAddFolderState(
    val parent: BookmarkItem.Folder,
    val folderBeingAddedTitle: String,
)

internal data class BookmarksEditFolderState(
    val parent: BookmarkItem.Folder,
    val folder: BookmarkItem.Folder,
)

internal data class SelectFolderItem(
    val indentation: Int,
    val folder: BookmarkItem.Folder,
) {
    val guid: String
        get() = folder.guid

    val title: String
        get() = folder.title

    val isDesktopRoot: Boolean
        get() = guid == BookmarkRoot.Root.id
}

internal data class BookmarksSelectFolderState(
    val selectionGuid: String? = null,
    val folderSelectionGuid: String? = null,
    val folders: List<SelectFolderItem> = listOf(),
) {
    val showNewFolderButton: Boolean
        get() = folderSelectionGuid == null

    val selectedGuid: String?
        get() = folderSelectionGuid ?: selectionGuid
}

internal val BookmarkItem.Folder.isDesktopFolder: Boolean
    get() = when (guid) {
        BookmarkRoot.Root.id,
        BookmarkRoot.Menu.id,
        BookmarkRoot.Toolbar.id,
        BookmarkRoot.Unfiled.id,
        -> true
        else -> false
    }

internal val BookmarkItem.Folder.isDesktopRoot: Boolean
    get() = guid == BookmarkRoot.Root.id
