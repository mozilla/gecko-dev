/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

import mozilla.appservices.places.BookmarkRoot

/**
 * Function for reducing a new bookmarks state based on the received action.
 */
@Suppress("LongMethod")
internal fun bookmarksReducer(state: BookmarksState, action: BookmarksAction) = when (action) {
    is InitEditLoaded -> state.copy(
        currentFolder = action.folder,
        bookmarksEditBookmarkState = BookmarksEditBookmarkState(
            bookmark = action.bookmark,
            folder = action.folder,
        ),
    )
    is BookmarksLoaded -> state.copy(
        currentFolder = action.folder,
        bookmarkItems = action.bookmarkItems,
    )
    is RecursiveSelectionCountLoaded -> state.copy(recursiveSelectedCount = action.count)
    is BookmarkLongClicked -> state.toggleSelectionOf(action.item)
    is FolderLongClicked -> state.toggleSelectionOf(action.item)
    is FolderClicked -> when {
        state.selectedItems.isNotEmpty() && action.item.isDesktopFolder -> state.copy(
            bookmarksSnackbarState = BookmarksSnackbarState.CantEditDesktopFolders,
        )
        state.selectedItems.isNotEmpty() -> state.toggleSelectionOf(action.item)
        else -> state
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
    is AddFolderAction.FolderCreated -> state.copy(
        bookmarksSelectFolderState = null,
        bookmarksEditBookmarkState = state.bookmarksEditBookmarkState?.copy(
            folder = action.folder,
        ),
    )
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
        ) ?: BookmarksSelectFolderState(folders = action.folders, outerSelectionGuid = BookmarkRoot.Mobile.id),
    )
    AddFolderClicked -> state.copy(
        bookmarksAddFolderState = BookmarksAddFolderState(
            parent = state.currentFolder,
            folderBeingAddedTitle = "",
        ),
    )
    is SelectFolderAction.ItemClicked -> state.updateSelectedFolder(action.folder)
    EditBookmarkAction.DeleteClicked -> state.copy(
        bookmarksSnackbarState = state.bookmarksEditBookmarkState?.let {
            state.bookmarksSnackbarState.addGuidToDelete(it.bookmark.guid)
        } ?: BookmarksSnackbarState.None,
        bookmarksEditBookmarkState = null,
    )
    BackClicked -> state.respondToBackClick()
    EditBookmarkAction.FolderClicked -> state.copy(
        bookmarksSelectFolderState = BookmarksSelectFolderState(
            outerSelectionGuid = state.bookmarksEditBookmarkState?.folder?.guid ?: state.currentFolder.guid,
        ),
    )
    AddFolderAction.ParentFolderClicked -> state.copy(
        bookmarksSelectFolderState = state.bookmarksSelectFolderState?.copy(
            innerSelectionGuid = state.bookmarksAddFolderState?.parent?.guid ?: state.currentFolder.guid,
        ) ?: BookmarksSelectFolderState(
            outerSelectionGuid = state.bookmarksAddFolderState?.parent?.guid ?: state.currentFolder.guid,
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
        bookmarksSelectFolderState = state.bookmarksSelectFolderState?.copy(
            innerSelectionGuid = state.bookmarksEditFolderState?.parent?.guid ?: state.currentFolder.guid,
        ) ?: BookmarksSelectFolderState(
            outerSelectionGuid = state.bookmarksEditFolderState?.parent?.guid ?: state.currentFolder.guid,
        ),
    )
    EditFolderAction.DeleteClicked -> state.bookmarksEditFolderState?.folder?.guid?.let {
        state.copy(
            bookmarksDeletionDialogState = DeletionDialogState.LoadingCount(
                listOf(state.bookmarksEditFolderState.folder.guid),
            ),
        )
    } ?: state
    is BookmarksListMenuAction -> state.handleListMenuAction(action)
    SnackbarAction.Undo -> state.copy(bookmarksSnackbarState = BookmarksSnackbarState.None)
    SnackbarAction.Dismissed -> {
        state.withDeletedItemsRemoved().copy(bookmarksSnackbarState = BookmarksSnackbarState.None)
    }
    is DeletionDialogAction.CountLoaded -> state.copy(
        bookmarksDeletionDialogState = DeletionDialogState.Presenting(
            guidsToDelete = state.bookmarksDeletionDialogState.guidsToDelete,
            recursiveCount = action.count,
        ),
    )
    DeletionDialogAction.CancelTapped -> state.copy(bookmarksDeletionDialogState = DeletionDialogState.None)
    DeletionDialogAction.DeleteTapped -> {
        state.withDeletedItemsRemoved().copy(bookmarksDeletionDialogState = DeletionDialogState.None)
    }
    is OpenTabsConfirmationDialogAction.Present -> state.copy(
        openTabsConfirmationDialog = OpenTabsConfirmationDialog.Presenting(
            guidToOpen = action.guid,
            numberOfTabs = action.count,
            isPrivate = action.isPrivate,
        ),
    )
    is ReceivedSyncSignInUpdate -> {
        state.copy(isSignedIntoSync = action.isSignedIn)
    }
    CloseClicked,
    OpenTabsConfirmationDialogAction.CancelTapped,
    OpenTabsConfirmationDialogAction.ConfirmTapped,
    -> state.copy(openTabsConfirmationDialog = OpenTabsConfirmationDialog.None)
    FirstSyncCompleted,
    ViewDisposed,
    SelectFolderAction.ViewAppeared,
    SearchClicked,
    SignIntoSyncClicked,
    is InitEdit,
    Init,
    -> state
}

private fun BookmarksState.withDeletedItemsRemoved(): BookmarksState = when {
    bookmarksDeletionDialogState is DeletionDialogState.Presenting -> copy(
        bookmarkItems = bookmarkItems.filterNot { bookmarksDeletionDialogState.guidsToDelete.contains(it.guid) },
    )
    bookmarksSnackbarState is BookmarksSnackbarState.UndoDeletion -> copy(
        bookmarkItems = bookmarkItems.filterNot { bookmarksSnackbarState.guidsToDelete.contains(it.guid) },
    )
    else -> this
}

private fun BookmarksState.updateSelectedFolder(folder: SelectFolderItem): BookmarksState = when {
    bookmarksSelectFolderState?.innerSelectionGuid != null -> {
        // we can't have both an add and edit folder at the same time, so we will just try to update
        // both of them.
        copy(
            bookmarksEditFolderState = bookmarksEditFolderState?.copy(parent = folder.folder),
            bookmarksAddFolderState = bookmarksAddFolderState?.copy(parent = folder.folder),
            bookmarksSelectFolderState = bookmarksSelectFolderState.copy(innerSelectionGuid = folder.guid),
        )
    }
    bookmarksSelectFolderState?.outerSelectionGuid != null -> {
        val alwaysTryUpdate = copy(
            bookmarksMultiselectMoveState = bookmarksMultiselectMoveState?.copy(destination = folder.guid),
            bookmarksSelectFolderState = bookmarksSelectFolderState.copy(outerSelectionGuid = folder.guid),
        )
        if (bookmarksEditBookmarkState == null) {
            alwaysTryUpdate.copy(
                bookmarksEditFolderState = bookmarksEditFolderState?.copy(parent = folder.folder),
                bookmarksAddFolderState = bookmarksAddFolderState?.copy(parent = folder.folder),
            )
        } else {
            alwaysTryUpdate.copy(
                bookmarksEditBookmarkState = bookmarksEditBookmarkState.copy(folder = folder.folder),
            )
        }
    }

    else -> this
}

private fun BookmarksState.toggleSelectionOf(item: BookmarkItem): BookmarksState =
    if (selectedItems.any { it.guid == item.guid }) {
        copy(selectedItems = selectedItems - item)
    } else {
        copy(selectedItems = selectedItems + item)
    }

private fun BookmarksState.respondToBackClick(): BookmarksState = when {
    // we check select folder state first because it can be the most deeply nested, e.g.
    // select -> add -> select
    bookmarksSelectFolderState != null -> {
        when {
            bookmarksSelectFolderState.innerSelectionGuid != null -> {
                copy(
                    bookmarksSelectFolderState = bookmarksSelectFolderState.copy(innerSelectionGuid = null),
                )
            }
            bookmarksAddFolderState != null && bookmarksEditBookmarkState != null -> {
                copy(bookmarksAddFolderState = null)
            }
            else -> copy(
                bookmarksMultiselectMoveState = null,
                bookmarksSelectFolderState = null,
            )
        }
    }
    bookmarksAddFolderState != null -> {
        copy(bookmarksAddFolderState = null)
    }
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
        BookmarksListMenuAction.MultiSelect.DeleteClicked -> {
            if (this.selectedItems.size > 1 || this.selectedItems.any { it is BookmarkItem.Folder }) {
                copy(
                    bookmarksDeletionDialogState = DeletionDialogState.LoadingCount(this.selectedItems.map { it.guid }),
                )
            } else {
                copy(
                    bookmarksSnackbarState = bookmarksSnackbarState.addGuidsToDelete(
                        guids = this.selectedItems.map { it.guid },
                    ),
                )
            }
        }
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
        is BookmarksListMenuAction.Bookmark.DeleteClicked -> copy(
            bookmarksSnackbarState = bookmarksSnackbarState.addGuidToDelete(action.bookmark.guid),
        )
        is BookmarksListMenuAction.Folder.DeleteClicked -> copy(
            bookmarksDeletionDialogState = DeletionDialogState.LoadingCount(listOf(action.folder.guid)),
        )
        BookmarksListMenuAction.MultiSelect.MoveClicked -> copy(
            bookmarksSelectFolderState = BookmarksSelectFolderState(
                outerSelectionGuid = currentFolder.guid,
            ),
            bookmarksMultiselectMoveState = MultiselectMoveState(
                guidsToMove = selectedItems.map { it.guid },
                destination = currentFolder.guid,
            ),
        )
        else -> this
    }.let { updatedState ->
        if (action is BookmarksListMenuAction.MultiSelect) {
            updatedState.copy(
                selectedItems = listOf(),
                recursiveSelectedCount = null,
            )
        } else {
            updatedState
        }
    }
