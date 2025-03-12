/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen.middleware

import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.distinctUntilChangedBy
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.downloads.toMegabyteOrKilobyteString
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Store
import mozilla.components.lib.state.ext.flow
import org.mozilla.fenix.downloads.listscreen.store.CreatedTime
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIAction
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIState
import org.mozilla.fenix.downloads.listscreen.store.FileItem
import java.io.File
import java.time.Instant
import java.time.ZoneId

/**
 * Middleware for loading and mapping download items from the browser store.
 *
 * @param browserStore [BrowserStore] instance to get the download items from.
 * @param scope The [CoroutineScope] that will be used to launch coroutines.
 * @param ioDispatcher The [CoroutineDispatcher] that will be used for IO operations.
 * @param dateTimeProvider The [DateTimeProvider] that will be used to get the current date.
 */
class DownloadUIMapperMiddleware(
    private val browserStore: BrowserStore,
    private val scope: CoroutineScope,
    private val ioDispatcher: CoroutineDispatcher = Dispatchers.IO,
    private val dateTimeProvider: DateTimeProvider = DateTimeProviderImpl(),
) : Middleware<DownloadUIState, DownloadUIAction> {

    override fun invoke(
        context: MiddlewareContext<DownloadUIState, DownloadUIAction>,
        next: (DownloadUIAction) -> Unit,
        action: DownloadUIAction,
    ) {
        next(action)
        when (action) {
            is DownloadUIAction.Init -> update(context.store)
            else -> {
                // no - op
            }
        }
    }

    private fun update(store: Store<DownloadUIState, DownloadUIAction>) {
        scope.launch {
            browserStore.flow()
                .distinctUntilChangedBy { it.downloads }
                .map { it.downloads.toFileItemsList() }
                .map { it.filterExistsOnDisk(ioDispatcher) }
                .collect {
                    store.dispatch(DownloadUIAction.UpdateFileItems(it))
                }
        }
    }

    private fun Map<String, DownloadState>.toFileItemsList(): List<FileItem> =
        values
            .distinctBy { it.fileName }
            .sortedByDescending { it.createdTime } // sort from newest to oldest
            .map { it.toFileItem() }
            .filter { it.status == DownloadState.Status.COMPLETED }

    private fun DownloadState.toFileItem() =
        FileItem(
            id = id,
            url = url,
            fileName = fileName,
            filePath = filePath,
            formattedSize = contentLength?.toMegabyteOrKilobyteString() ?: "0",
            contentType = contentType,
            status = status,
            createdTime = categorizeTime(createdTime),
        )

    private fun categorizeTime(epochMillis: Long): CreatedTime {
        val currentDate = dateTimeProvider.currentLocalDate()
        val inputDate = Instant.ofEpochMilli(epochMillis)
            .atZone(ZoneId.systemDefault())
            .toLocalDate()

        return when {
            inputDate.isEqual(currentDate) -> CreatedTime.TODAY
            inputDate.isEqual(currentDate.minusDays(1)) -> CreatedTime.YESTERDAY
            inputDate.isAfter(currentDate.minusDays(NUM_DAYS_IN_LAST_7_DAYS_PERIOD)) -> CreatedTime.LAST_7_DAYS
            inputDate.isAfter(currentDate.minusDays(NUM_DAYS_IN_LAST_30_DAYS_PERIOD)) -> CreatedTime.LAST_30_DAYS
            else -> CreatedTime.OLDER
        }
    }

    /**
     * Constants for [DownloadUIMapperMiddleware].
     */
    companion object {
        private const val NUM_DAYS_IN_LAST_7_DAYS_PERIOD = 7L
        private const val NUM_DAYS_IN_LAST_30_DAYS_PERIOD = 30L
    }
}

/**
 * Returns a filtered list of [FileItem]s containing only items that are present on the disk.
 * If a user has deleted the downloaded item it should not show on the downloaded list.
 */
suspend fun List<FileItem>.filterExistsOnDisk(dispatcher: CoroutineDispatcher): List<FileItem> =
    withContext(dispatcher) {
        filter {
            File(it.filePath).exists()
        }
    }
