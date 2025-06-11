/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen.store

import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.Store
import mozilla.components.lib.state.UiStore
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIState.Mode

/**
 * The [Store] for holding the [DownloadUIState] and applying [DownloadUIAction]s.
 */
class DownloadUIStore(
    initialState: DownloadUIState,
    middleware: List<Middleware<DownloadUIState, DownloadUIAction>> = emptyList(),
) : UiStore<DownloadUIState, DownloadUIAction>(
    initialState = initialState,
    reducer = ::downloadStateReducer,
    middleware = middleware,
) {

    init {
        dispatch(DownloadUIAction.Init)
    }
}

/**
 * The DownloadState Reducer.
 */
@Suppress("LongMethod")
private fun downloadStateReducer(
    state: DownloadUIState,
    action: DownloadUIAction,
): DownloadUIState {
    return when (action) {
        is DownloadUIAction.AddItemForRemoval ->
            state.copy(
                mode = Mode.Editing(state.mode.selectedItems + action.item),
            )

        is DownloadUIAction.AddAllItemsForRemoval -> {
            state.copy(
                mode = Mode.Editing(state.itemsMatchingFilters.toSet()),
            )
        }

        is DownloadUIAction.RemoveItemForRemoval -> {
            val selected = state.mode.selectedItems - action.item
            state.copy(
                mode = if (selected.isEmpty()) {
                    Mode.Normal
                } else {
                    Mode.Editing(selected)
                },
            )
        }

        is DownloadUIAction.ExitEditMode -> state.copy(mode = Mode.Normal)
        is DownloadUIAction.AddPendingDeletionSet ->
            state.copy(pendingDeletionIds = state.pendingDeletionIds + action.itemIds)

        is DownloadUIAction.UndoPendingDeletionSet ->
            state.copy(pendingDeletionIds = state.pendingDeletionIds - action.itemIds)

        is DownloadUIAction.UpdateFileItems -> state.copy(items = action.items)

        is DownloadUIAction.ContentTypeSelected -> state.copy(userSelectedContentTypeFilter = action.contentTypeFilter)

        is DownloadUIAction.FileItemDeletedSuccessfully -> state

        is DownloadUIAction.SearchQueryEntered -> state.copy(searchQuery = action.searchQuery)
        is DownloadUIAction.UpdateDeleteDialogVisibility -> state.copy(isDeleteDialogVisible = action.visibility)

        DownloadUIAction.Init -> state
        is DownloadUIAction.ShareUrlClicked -> state
        is DownloadUIAction.ShareFileClicked -> state
        is DownloadUIAction.UndoPendingDeletion -> state
        is DownloadUIAction.PauseDownload -> {
            state.copyWithFileItemStatusTransition(
                downloadId = action.downloadId,
                downloadControlAction = FileItem.Status.DownloadControlAction.PAUSE,
            )
        }

        is DownloadUIAction.ResumeDownload -> {
            state.copyWithFileItemStatusTransition(
                downloadId = action.downloadId,
                downloadControlAction = FileItem.Status.DownloadControlAction.RESUME,
            )
        }

        is DownloadUIAction.RetryDownload -> {
            state.copyWithFileItemStatusTransition(
                downloadId = action.downloadId,
                downloadControlAction = FileItem.Status.DownloadControlAction.RETRY,
            )
        }

        is DownloadUIAction.CancelDownload -> {
            state.copyWithFileItemStatusTransition(
                downloadId = action.downloadId,
                downloadControlAction = FileItem.Status.DownloadControlAction.CANCEL,
            )
        }

        is DownloadUIAction.SearchBarDismissRequest -> state.copy(
            isSearchFieldRequested = false,
            searchQuery = "",
        )

        is DownloadUIAction.SearchBarVisibilityRequest -> state.copy(isSearchFieldRequested = true)
    }
}

private fun DownloadUIState.copyWithFileItemStatusTransition(
    downloadId: String,
    downloadControlAction: FileItem.Status.DownloadControlAction,
): DownloadUIState {
    val itemIndex = items.indexOfFirst { it.id == downloadId }
    if (itemIndex == -1) {
        return this
    }

    val updatedItems = items.map {
        if (it.id == downloadId) {
            it.copy(status = it.status.transition(action = downloadControlAction))
        } else {
            it
        }
    }
    return copy(items = updatedItems)
}
