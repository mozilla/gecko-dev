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
 * @property recursiveSelectedCount the total number of children of the [selectedItems] found in bookmark storage.
 * @property currentFolder the [BookmarkItem.Folder] that is currently being displayed.
 * @property isSignedIntoSync State representing if the user is currently signed into sync.
 * @property openTabsConfirmationDialog State representing the confirmation dialog state.
 * @property bookmarksDeletionDialogState State representing the deletion dialog state.
 * @property bookmarksSnackbarState State representing which snackbar to show.
 * @property bookmarksAddFolderState State representing the add folder subscreen, if visible.
 * @property bookmarksEditBookmarkState State representing the edit bookmark subscreen, if visible.
 * @property bookmarksSelectFolderState State representing the select folder subscreen, if visible.
 * @property bookmarksEditFolderState State representing the edit folder subscreen, if visible.
 * @property bookmarksMultiselectMoveState State representing multi-select moving.
 */
internal data class BookmarksState(
    val bookmarkItems: List<BookmarkItem>,
    val selectedItems: List<BookmarkItem>,
    val recursiveSelectedCount: Int?,
    val currentFolder: BookmarkItem.Folder,
    val isSignedIntoSync: Boolean,
    val openTabsConfirmationDialog: OpenTabsConfirmationDialog,
    val bookmarksDeletionDialogState: DeletionDialogState,
    val bookmarksSnackbarState: BookmarksSnackbarState,
    val bookmarksAddFolderState: BookmarksAddFolderState?,
    val bookmarksEditBookmarkState: BookmarksEditBookmarkState?,
    val bookmarksSelectFolderState: BookmarksSelectFolderState?,
    val bookmarksEditFolderState: BookmarksEditFolderState?,
    val bookmarksMultiselectMoveState: MultiselectMoveState?,
) : State {
    val showNewFolderButton: Boolean
        get() = bookmarksSelectFolderState?.innerSelectionGuid == null &&
            bookmarksAddFolderState == null && bookmarksEditFolderState == null

    companion object {
        val default: BookmarksState = BookmarksState(
            bookmarkItems = listOf(),
            selectedItems = listOf(),
            recursiveSelectedCount = null,
            currentFolder = BookmarkItem.Folder("", ""),
            isSignedIntoSync = false,
            openTabsConfirmationDialog = OpenTabsConfirmationDialog.None,
            bookmarksSnackbarState = BookmarksSnackbarState.None,
            bookmarksDeletionDialogState = DeletionDialogState.None,
            bookmarksAddFolderState = null,
            bookmarksEditBookmarkState = null,
            bookmarksSelectFolderState = null,
            bookmarksEditFolderState = null,
            bookmarksMultiselectMoveState = null,
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
            val title = this.bookmarkItems.firstOrNull { it.guid == state.guidsToDelete.first() }?.title
            stringId to (title ?: "error")
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

internal fun BookmarksState.isGuidBeingMoved(guid: String): Boolean {
    return bookmarksMultiselectMoveState?.guidsToMove?.contains(guid) ?: false ||
        bookmarksEditFolderState?.folder?.guid == guid
}

internal data class MultiselectMoveState(
    val guidsToMove: List<String>,
    val destination: String,
)

internal sealed class DeletionDialogState {
    data object None : DeletionDialogState()
    data class LoadingCount(val guidsToDelete: List<String>) : DeletionDialogState()
    data class Presenting(
        val guidsToDelete: List<String>,
        val recursiveCount: Int,
    ) : DeletionDialogState()
}

internal sealed class OpenTabsConfirmationDialog {
    data object None : OpenTabsConfirmationDialog()
    data class Presenting(
        val guidToOpen: String,
        val numberOfTabs: Int,
        val isPrivate: Boolean,
    ) : OpenTabsConfirmationDialog()
}

internal val DeletionDialogState.Presenting.count
    get() = guidsToDelete.size + recursiveCount

internal val DeletionDialogState.guidsToDelete: List<String>
    get() = when (this) {
        DeletionDialogState.None -> listOf()
        is DeletionDialogState.LoadingCount -> guidsToDelete
        is DeletionDialogState.Presenting -> guidsToDelete
    }

internal sealed class BookmarksSnackbarState {
    data object None : BookmarksSnackbarState()
    data object CantEditDesktopFolders : BookmarksSnackbarState()
    data class UndoDeletion(val guidsToDelete: List<String>) : BookmarksSnackbarState()
}

internal fun BookmarksSnackbarState.addGuidToDelete(guid: String) = when (this) {
    is BookmarksSnackbarState.UndoDeletion -> BookmarksSnackbarState.UndoDeletion(
        guidsToDelete = this.guidsToDelete + listOf(guid),
    )
    else -> BookmarksSnackbarState.UndoDeletion(guidsToDelete = listOf(guid))
}

internal fun BookmarksSnackbarState.addGuidsToDelete(guids: List<String>) = when (this) {
    is BookmarksSnackbarState.UndoDeletion -> BookmarksSnackbarState.UndoDeletion(
        guidsToDelete = this.guidsToDelete + guids,
    )
    else -> BookmarksSnackbarState.UndoDeletion(guidsToDelete = guids)
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

/**
 * State representing the select folder subscreen.
 *
 * @property outerSelectionGuid The currently selected folder guid for the initial select folder screen.
 * Required since there is always at least this property active while the screen is visible.
 * @property innerSelectionGuid If in the select folder -> add folder -> select folder flow,
 * this represents the selection GUID for the nest select screen where the newly added folder is being
 * placed. Optional since this screen may never be displayed.
 * @property folders The folders to display.
 */
internal data class BookmarksSelectFolderState(
    val outerSelectionGuid: String,
    val innerSelectionGuid: String? = null,
    val folders: List<SelectFolderItem> = listOf(),
) {
    val selectedGuid: String
        get() = innerSelectionGuid ?: outerSelectionGuid
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

internal val BookmarkItem.Folder.isMobileRoot: Boolean
    get() = guid == BookmarkRoot.Mobile.id

internal val BookmarkItem.Folder.isDesktopRoot: Boolean
    get() = guid == BookmarkRoot.Root.id
