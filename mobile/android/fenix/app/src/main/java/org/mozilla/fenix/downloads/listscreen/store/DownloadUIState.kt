/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen.store

import mozilla.components.lib.state.State

/**
 * The state of the Download screen.
 *
 * @property items List of [FileItem] to display.
 * @property mode Current [Mode] of the Download screen.
 * @property pendingDeletionIds Set of [FileItem] IDs that are waiting to be deleted.
 * @property isDeletingItems Whether or not download items are being deleted.
 */
data class DownloadUIState(
    val items: List<FileItem>,
    val mode: Mode,
    val pendingDeletionIds: Set<String>,
    val isDeletingItems: Boolean,
) : State {

    /**
     * The ungrouped list of items to display, excluding any items that are pending deletion.
     */
    val itemsNotPendingDeletion = items.filter { it.id !in pendingDeletionIds }

    /**
     * The list of items to display grouped by the created time of the item.
     */
    val itemsToDisplay: List<DownloadListItem> = itemsNotPendingDeletion
        .groupBy { it.createdTime }
        .flatMap { (key, value) ->
            listOf(HeaderItem(key)) + value
        }

    /**
     * Whether or not the state is in an empty state.
     */
    val isEmptyState: Boolean = itemsToDisplay.isEmpty()

    /**
     * Whether or not the state is in normal mode.
     */
    val isNormalMode: Boolean = mode is Mode.Normal

    /**
     * @see [DownloadUIState].
     */
    companion object {
        val INITIAL = DownloadUIState(
            items = emptyList(),
            mode = Mode.Normal,
            pendingDeletionIds = emptySet(),
            isDeletingItems = false,
        )
    }

    /**
     * The mode of the Download screen.
     */
    sealed class Mode {
        open val selectedItems = emptySet<FileItem>()

        /**
         * Normal mode for the Download screen.
         */
        data object Normal : Mode()

        /**
         * Editing mode for the Download screen where items can be selected.
         */
        data class Editing(override val selectedItems: Set<FileItem>) : Mode()
    }
}
