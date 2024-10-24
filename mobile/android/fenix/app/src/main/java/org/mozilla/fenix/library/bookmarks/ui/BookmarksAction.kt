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
internal data class InitEdit(val guid: String) : BookmarksAction
internal data class InitEditLoaded(
    val bookmark: BookmarkItem.Bookmark,
    val folder: BookmarkItem.Folder,
) : BookmarksAction
internal data object ViewDisposed : BookmarksAction

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

internal data class RecursiveSelectionCountLoaded(
    val count: Int,
) : BookmarksAction

internal sealed class BookmarksListMenuAction : BookmarksAction {
    internal sealed class Bookmark : BookmarksListMenuAction() {
        data class EditClicked(val bookmark: BookmarkItem.Bookmark) : Bookmark()
        data class CopyClicked(val bookmark: BookmarkItem.Bookmark) : Bookmark()
        data class ShareClicked(val bookmark: BookmarkItem.Bookmark) : Bookmark()
        data class OpenInNormalTabClicked(val bookmark: BookmarkItem.Bookmark) : Bookmark()
        data class OpenInPrivateTabClicked(val bookmark: BookmarkItem.Bookmark) : Bookmark()
        data class DeleteClicked(val bookmark: BookmarkItem.Bookmark) : Bookmark()
    }
    internal sealed class Folder : BookmarksListMenuAction() {
        data class EditClicked(val folder: BookmarkItem.Folder) : Folder()
        data class OpenAllInNormalTabClicked(val folder: BookmarkItem.Folder) : Folder()
        data class OpenAllInPrivateTabClicked(val folder: BookmarkItem.Folder) : Folder()
        data class DeleteClicked(val folder: BookmarkItem.Folder) : Folder()
    }
    internal sealed class MultiSelect : BookmarksListMenuAction() {
        data object EditClicked : MultiSelect()
        data object MoveClicked : MultiSelect()
        data object DeleteClicked : MultiSelect()
        data object OpenInNormalTabsClicked : MultiSelect()
        data object OpenInPrivateTabsClicked : MultiSelect()
        data object ShareClicked : MultiSelect()
    }
}

internal data class FolderClicked(val item: BookmarkItem.Folder) : BookmarksAction
internal data class FolderLongClicked(val item: BookmarkItem.Folder) : BookmarksAction
internal data class BookmarkClicked(val item: BookmarkItem.Bookmark) : BookmarksAction
internal data class BookmarkLongClicked(val item: BookmarkItem.Bookmark) : BookmarksAction
internal data object SearchClicked : BookmarksAction
internal data object AddFolderClicked : BookmarksAction
internal data object CloseClicked : BookmarksAction
internal data object BackClicked : BookmarksAction
internal data object SignIntoSyncClicked : BookmarksAction
internal data class EditBookmarkClicked(val bookmark: BookmarkItem.Bookmark) : BookmarksAction
internal data class ReceivedSyncSignInUpdate(val isSignedIn: Boolean) : BookmarksAction
internal data object FirstSyncCompleted : BookmarksAction

/**
 * Actions specific to the Add Folder screen.
 */
internal sealed class AddFolderAction {
    data class TitleChanged(val updatedText: String) : BookmarksAction
    data object ParentFolderClicked : BookmarksAction
    data class FolderCreated(val folder: BookmarkItem.Folder) : BookmarksAction
}

/**
 * Actions specific to the Edit Folder screen.
 */
internal sealed class EditFolderAction : BookmarksAction {
    data class TitleChanged(val updatedText: String) : BookmarksAction
    data object ParentFolderClicked : BookmarksAction
    data object DeleteClicked : EditFolderAction()
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
    data class ItemClicked(val folder: SelectFolderItem) : BookmarksAction
}

internal sealed class OpenTabsConfirmationDialogAction {
    data class Present(
        val guid: String,
        val count: Int,
        val isPrivate: Boolean,
    ) : BookmarksAction
    data object ConfirmTapped : BookmarksAction
    data object CancelTapped : BookmarksAction
}

internal sealed class DeletionDialogAction {
    data class CountLoaded(val count: Int) : BookmarksAction
    data object CancelTapped : BookmarksAction
    data object DeleteTapped : BookmarksAction
}

internal sealed class SnackbarAction {
    data object Undo : BookmarksAction
    data object Dismissed : BookmarksAction
}
