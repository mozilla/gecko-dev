/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

import mozilla.components.lib.state.State

/**
 * Represents the state of the Bookmarks list screen and its various subscreens.
 *
 * @property bookmarkItems Bookmark items to be displayed in the current list screen.
 * @property selectedItems The bookmark items that are currently selected by the user for bulk actions.
 * @property currentFolder the [BookmarkItem.Folder] that is currently being displayed.
 * @property isSignedIntoSync State representing if the user is currently signed into sync.
 * @property bookmarksAddFolderState State representing the add folder subscreen, if visible.
 * @property bookmarksEditBookmarkState State representing the edit bookmark subscreen, if visible.
 * @property bookmarksSelectFolderState State representing the select folder subscreen, if visible.
 */
internal data class BookmarksState(
    val bookmarkItems: List<BookmarkItem>,
    val selectedItems: List<BookmarkItem>,
    val currentFolder: BookmarkItem.Folder,
    val isSignedIntoSync: Boolean,
    val bookmarksAddFolderState: BookmarksAddFolderState?,
    val bookmarksEditBookmarkState: BookmarksEditBookmarkState?,
    val bookmarksSelectFolderState: BookmarksSelectFolderState?,
) : State {
    companion object {
        val default: BookmarksState = BookmarksState(
            bookmarkItems = listOf(),
            selectedItems = listOf(),
            currentFolder = BookmarkItem.Folder("", ""),
            isSignedIntoSync = false,
            bookmarksAddFolderState = null,
            bookmarksEditBookmarkState = null,
            bookmarksSelectFolderState = null,
        )
    }
}

internal data class BookmarksEditBookmarkState(
    val bookmark: BookmarkItem.Bookmark,
    val folder: BookmarkItem.Folder,
)

internal data class BookmarksAddFolderState(
    val parent: BookmarkItem.Folder,
    val folderBeingAddedTitle: String,
)

internal data class SelectFolderItem(
    val indentation: Int,
    val folder: BookmarkItem.Folder,
) {
    val guid: String
        get() = folder.guid

    val title: String
        get() = folder.title
}

internal data class BookmarksSelectFolderState(
    val selectionGuid: String? = null,
    val addFolderSelectionGuid: String? = null,
    val folders: List<SelectFolderItem> = listOf(),
) {
    val showNewFolderButton: Boolean
        get() = addFolderSelectionGuid == null
}
