/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

import mozilla.components.lib.state.Action

/**
 * Actions relating to the Bookmarks list screen and its various subscreens.
 */
internal sealed interface BookmarksAction : Action

/**
 * The Store is initializing.
 */
internal data object Init : BookmarksAction

/**
 * Bookmarks have been loaded from the storage layer.
 *
 * @property folder The loaded [BookmarkItem.Folder]
 * @property bookmarkItems The bookmark items loaded, transformed into a displayable type.
 */
internal data class BookmarksLoaded(
    val folder: BookmarkItem.Folder,
    val bookmarkItems: List<BookmarkItem>,
) : BookmarksAction

internal data class FolderClicked(val item: BookmarkItem.Folder) : BookmarksAction
internal data class FolderLongClicked(val item: BookmarkItem.Folder) : BookmarksAction
internal data class BookmarkClicked(val item: BookmarkItem.Bookmark) : BookmarksAction
internal data class BookmarkLongClicked(val item: BookmarkItem.Bookmark) : BookmarksAction
internal data object SearchClicked : BookmarksAction
internal data object AddFolderClicked : BookmarksAction
internal data object BackClicked : BookmarksAction
internal data object SignIntoSyncClicked : BookmarksAction
internal data class EditBookmarkClicked(val bookmark: BookmarkItem.Bookmark) : BookmarksAction

/**
 * Actions specific to the Add Folder screen.
 */
internal sealed class AddFolderAction {
    data class TitleChanged(val updatedText: String) : BookmarksAction
    data object ParentFolderClicked : BookmarksAction
}

internal sealed class EditBookmarkAction {
    data class TitleChanged(val title: String) : BookmarksAction
    data class URLChanged(val url: String) : BookmarksAction
    data object FolderClicked : BookmarksAction
    data object DeleteClicked : BookmarksAction
}

internal sealed class SelectFolderAction {
    data object ViewAppeared : BookmarksAction
    data class FoldersLoaded(val folders: List<SelectFolderItem>) : BookmarksAction
}
