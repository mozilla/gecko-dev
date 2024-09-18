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
 * @property folderTitle The title of currently selected folder whose children items are being displayed.
 * @property folderGuid The unique GUID representing the currently selected folder in storage.
 * @property bookmarksAddFolderState State representing the add folder subscreen, if visible.
 */
internal data class BookmarksState(
    val bookmarkItems: List<BookmarkItem>,
    val selectedItems: List<BookmarkItem>,
    val folderTitle: String,
    val folderGuid: String,
    val bookmarksAddFolderState: BookmarksAddFolderState?,
) : State {
    companion object {
        val default: BookmarksState = BookmarksState(
            bookmarkItems = listOf(),
            selectedItems = listOf(),
            folderTitle = "",
            folderGuid = "",
            bookmarksAddFolderState = null,
        )
    }
}

internal data class BookmarksAddFolderState(
    val folderBeingAddedTitle: String,
)
