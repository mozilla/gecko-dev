/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen.store

import mozilla.components.lib.state.State
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIState.Mode

/**
 * The state of the Download screen.
 *
 * @property items List of [FileItem] to display.
 * @property mode Current [Mode] of the Download screen.
 * @property pendingDeletionIds Set of [FileItem] IDs that are waiting to be deleted.
 * @property searchQuery The search query entered by the user.
 * @property isDeleteDialogVisible Flag indicating whether the delete confirmation dialog is currently visible.
 * @param isSearchEnabled Feature flag for search functionality.
 * @param userSelectedContentTypeFilter The user selected [FileItem.ContentTypeFilter].
 */
data class DownloadUIState(
    val items: List<FileItem>,
    val mode: Mode,
    val pendingDeletionIds: Set<String>,
    val searchQuery: String = "",
    val isDeleteDialogVisible: Boolean = false,
    private val isSearchEnabled: Boolean = true,
    private val userSelectedContentTypeFilter: FileItem.ContentTypeFilter = FileItem.ContentTypeFilter.All,
) : State {

    /**
     * The ungrouped list of items to display, excluding any items that are pending deletion.
     */
    val itemsNotPendingDeletion = items.filter { it.id !in pendingDeletionIds }

    /**
     * The content type filter that is actually used to filter the items. This overrides the user
     * selected content type filter to [FileItem.ContentTypeFilter.All] if there are no items that
     * matches the user selected filter.
     */
    val selectedContentTypeFilter: FileItem.ContentTypeFilter
        get() {
            val selectedTypeContainsItems = itemsNotPendingDeletion
                .any { download -> userSelectedContentTypeFilter.predicate(download.contentType) }

            return if (selectedTypeContainsItems) {
                userSelectedContentTypeFilter
            } else {
                FileItem.ContentTypeFilter.All
            }
        }

    private val searchQueryPredicate: (FileItem) -> Boolean = {
        if (isSearchEnabled) {
            it.stringToMatchForSearchQuery.contains(searchQuery, ignoreCase = true)
        } else {
            true
        }
    }

    /**
     * The list of items to display grouped by the created time of the item.
     */
    val itemsToDisplay: List<DownloadListItem> = itemsNotPendingDeletion
        .filter { selectedContentTypeFilter.predicate(it.contentType) }
        .filter(searchQueryPredicate)
        .groupBy { it.createdTime }
        .toSortedMap()
        .flatMap { (key, value) ->
            listOf(HeaderItem(key)) + value
        }

    /**
     * The list of content type filters that have at least one item that matches the filter.
     */
    private val matchingFilters: List<FileItem.ContentTypeFilter> =
        itemsNotPendingDeletion
            .map { it.matchingContentTypeFilter }
            .distinct()
            .sorted()

    /**
     * The list of content type filters to display.
     */
    val filtersToDisplay: List<FileItem.ContentTypeFilter> =
        if (matchingFilters.size > 1) {
            listOf(FileItem.ContentTypeFilter.All) + matchingFilters
        } else {
            emptyList()
        }

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
            userSelectedContentTypeFilter = FileItem.ContentTypeFilter.All,
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
