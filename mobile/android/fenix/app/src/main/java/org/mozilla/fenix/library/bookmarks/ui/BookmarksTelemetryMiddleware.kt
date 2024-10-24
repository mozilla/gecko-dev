/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.BookmarksManagement
import org.mozilla.fenix.components.metrics.MetricsUtils

private const val EDIT_SCREEN_METRIC_SOURCE = "bookmark_edit_page"
private const val LIST_SCREEN_METRIC_SOURCE = "bookmark_panel"

internal class BookmarksTelemetryMiddleware : Middleware<BookmarksState, BookmarksAction> {

    @Suppress("LongMethod", "ComplexMethod")
    override fun invoke(
        context: MiddlewareContext<BookmarksState, BookmarksAction>,
        next: (BookmarksAction) -> Unit,
        action: BookmarksAction,
    ) {
        val preReductionState = context.state
        next(action)
        when (action) {
            BackClicked -> preReductionState.handleBackClick()
            is BookmarksListMenuAction.Bookmark.CopyClicked -> {
                BookmarksManagement.copied.record(NoExtras())
            }
            DeletionDialogAction.DeleteTapped -> {
                val deletedItems = preReductionState.bookmarkItems.filter {
                    it.guid in preReductionState.bookmarksDeletionDialogState.guidsToDelete
                }
                if (deletedItems.any { it is BookmarkItem.Folder }) {
                    BookmarksManagement.folderRemove.record(NoExtras())
                }

                if (deletedItems.size > 1) {
                    BookmarksManagement.multiRemoved.record(NoExtras())
                }
            }
            is BookmarkClicked -> {
                if (preReductionState.selectedItems.isEmpty()) {
                    BookmarksManagement.open.record(NoExtras())
                    MetricsUtils.recordBookmarkMetrics(
                        MetricsUtils.BookmarkAction.OPEN,
                        LIST_SCREEN_METRIC_SOURCE,
                    )
                }
            }
            is BookmarksListMenuAction.Folder.OpenAllInNormalTabClicked -> {
                BookmarksManagement.openAllInNewTabs.record(NoExtras())
                MetricsUtils.recordBookmarkMetrics(
                    MetricsUtils.BookmarkAction.OPEN,
                    LIST_SCREEN_METRIC_SOURCE,
                )
            }
            is BookmarksListMenuAction.Folder.OpenAllInPrivateTabClicked -> {
                BookmarksManagement.openInPrivateTabs.record(NoExtras())
                MetricsUtils.recordBookmarkMetrics(
                    MetricsUtils.BookmarkAction.OPEN,
                    LIST_SCREEN_METRIC_SOURCE,
                )
            }
            is BookmarksListMenuAction.Bookmark.OpenInNormalTabClicked -> {
                BookmarksManagement.openInNewTab.record(NoExtras())
                MetricsUtils.recordBookmarkMetrics(
                    MetricsUtils.BookmarkAction.OPEN,
                    LIST_SCREEN_METRIC_SOURCE,
                )
            }
            is BookmarksListMenuAction.Bookmark.OpenInPrivateTabClicked -> {
                BookmarksManagement.openInPrivateTab.record(NoExtras())
                MetricsUtils.recordBookmarkMetrics(
                    MetricsUtils.BookmarkAction.OPEN,
                    LIST_SCREEN_METRIC_SOURCE,
                )
            }
            BookmarksListMenuAction.MultiSelect.OpenInNormalTabsClicked -> {
                BookmarksManagement.openInNewTabs.record(NoExtras())
                MetricsUtils.recordBookmarkMetrics(
                    MetricsUtils.BookmarkAction.OPEN,
                    LIST_SCREEN_METRIC_SOURCE,
                )
            }
            BookmarksListMenuAction.MultiSelect.OpenInPrivateTabsClicked -> {
                BookmarksManagement.openInPrivateTabs.record(NoExtras())
                MetricsUtils.recordBookmarkMetrics(
                    MetricsUtils.BookmarkAction.OPEN,
                    LIST_SCREEN_METRIC_SOURCE,
                )
            }
            SnackbarAction.Dismissed -> {
                val snackSnate = preReductionState.bookmarksSnackbarState
                if (snackSnate is BookmarksSnackbarState.UndoDeletion && snackSnate.guidsToDelete.size == 1) {
                    BookmarksManagement.removed.record(NoExtras())
                    val source = if (preReductionState.bookmarksEditFolderState != null) {
                        EDIT_SCREEN_METRIC_SOURCE
                    } else {
                        LIST_SCREEN_METRIC_SOURCE
                    }
                    MetricsUtils.recordBookmarkMetrics(MetricsUtils.BookmarkAction.DELETE, source)
                }
            }
            SearchClicked -> {
                BookmarksManagement.searchIconTapped.record(NoExtras())
            }
            is BookmarksListMenuAction.Bookmark.ShareClicked -> {
                BookmarksManagement.shared.record(NoExtras())
            }
            BookmarksListMenuAction.MultiSelect.ShareClicked -> {
                preReductionState.selectedItems.filterIsInstance<BookmarkItem.Bookmark>()
                    .forEach { BookmarksManagement.shared.record(NoExtras()) }
            }
            is BookmarksListMenuAction.Folder.DeleteClicked,
            CloseClicked,
            AddFolderClicked,
            is BookmarkLongClicked,
            is BookmarksListMenuAction.Bookmark.DeleteClicked,
            is BookmarksListMenuAction.Bookmark.EditClicked,
            is BookmarksListMenuAction.Folder.EditClicked,
            BookmarksListMenuAction.MultiSelect.DeleteClicked,
            BookmarksListMenuAction.MultiSelect.EditClicked,
            BookmarksListMenuAction.MultiSelect.MoveClicked,
            is BookmarksLoaded,
            EditBookmarkAction.DeleteClicked,
            is EditBookmarkClicked,
            is FolderClicked,
            EditBookmarkAction.FolderClicked,
            is FolderLongClicked,
            is SelectFolderAction.FoldersLoaded,
            Init,
            is SelectFolderAction.ItemClicked,
            AddFolderAction.ParentFolderClicked,
            SignIntoSyncClicked,
            is AddFolderAction.FolderCreated,
            is AddFolderAction.TitleChanged,
            is EditBookmarkAction.TitleChanged,
            is EditBookmarkAction.URLChanged,
            SelectFolderAction.ViewAppeared,
            DeletionDialogAction.CancelTapped,
            is DeletionDialogAction.CountLoaded,
            EditFolderAction.DeleteClicked,
            EditFolderAction.ParentFolderClicked,
            is RecursiveSelectionCountLoaded,
            is EditFolderAction.TitleChanged,
            SnackbarAction.Undo,
            OpenTabsConfirmationDialogAction.CancelTapped,
            OpenTabsConfirmationDialogAction.ConfirmTapped,
            is OpenTabsConfirmationDialogAction.Present,
            is InitEdit,
            is InitEditLoaded,
            is ReceivedSyncSignInUpdate,
            FirstSyncCompleted,
            ViewDisposed,
            -> Unit
        }
    }

    private fun BookmarksState.handleBackClick() {
        when {
            bookmarksEditBookmarkState != null -> {
                BookmarksManagement.edited.record(NoExtras())
                MetricsUtils.recordBookmarkMetrics(
                    MetricsUtils.BookmarkAction.EDIT,
                    EDIT_SCREEN_METRIC_SOURCE,
                )
                if (bookmarksEditBookmarkState.folder != currentFolder) {
                    BookmarksManagement.moved.record(NoExtras())
                }
            }

            bookmarksAddFolderState != null -> {
                if (bookmarksAddFolderState.folderBeingAddedTitle != "") {
                    BookmarksManagement.folderAdd.record(NoExtras())
                }
            }

            bookmarksSelectFolderState != null -> {
                if (bookmarksMultiselectMoveState != null &&
                    bookmarksMultiselectMoveState.destination != currentFolder.guid
                ) {
                    BookmarksManagement.moved.record(NoExtras())
                }
            }
        }
    }
}
