/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen.middleware

import androidx.annotation.FloatRange
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.flow.distinctUntilChangedBy
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import mozilla.components.browser.state.action.DownloadAction
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Store
import mozilla.components.lib.state.ext.flow
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIAction
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIState
import org.mozilla.fenix.downloads.listscreen.store.FileItem
import org.mozilla.fenix.downloads.listscreen.store.TimeCategory
import org.mozilla.fenix.ext.getBaseDomainUrl
import java.time.Instant

/**
 * Middleware for loading and mapping download items from the browser store.
 *
 * @param browserStore [BrowserStore] instance to get the download items from.
 * @param fileItemDescriptionProvider [FileItemDescriptionProvider] used to format the description
 * of the file item.
 * @param scope The [CoroutineScope] that will be used to launch coroutines.
 * @param dateTimeProvider The [DateTimeProvider] that will be used to get the current date.
 * @param isLiveDownloadsEnabled Whether or not live downloads in progress in the UI is enabled.
 */
class DownloadUIMapperMiddleware(
    private val browserStore: BrowserStore,
    private val fileItemDescriptionProvider: FileItemDescriptionProvider,
    private val scope: CoroutineScope,
    private val dateTimeProvider: DateTimeProvider = DateTimeProviderImpl(),
    private val isLiveDownloadsEnabled: Boolean = FeatureFlags.showLiveDownloads,
) : Middleware<DownloadUIState, DownloadUIAction> {

    override fun invoke(
        context: MiddlewareContext<DownloadUIState, DownloadUIAction>,
        next: (DownloadUIAction) -> Unit,
        action: DownloadUIAction,
    ) {
        next(action)
        when (action) {
            is DownloadUIAction.Init -> {
                browserStore.dispatch(DownloadAction.RemoveDeletedDownloads)
                update(context.store)
            }

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
                .collect {
                    store.dispatch(DownloadUIAction.UpdateFileItems(it))
                }
        }
    }

    private fun Map<String, DownloadState>.toFileItemsList(): List<FileItem> =
        values
            .distinctBy { it.fileName }
            .filter { isDisplayableItem(it.status) }
            .sortedByDescending { it.createdTime } // sort from newest to oldest
            .map { it.toFileItem() }

    private fun isDisplayableItem(status: DownloadState.Status) =
        status == DownloadState.Status.COMPLETED || isLiveDownloadsEnabled &&
            status != DownloadState.Status.CANCELLED

    private fun DownloadState.toFileItem() =
        FileItem(
            id = id,
            url = url,
            fileName = fileName,
            filePath = filePath,
            displayedShortUrl = url.getBaseDomainUrl(),
            contentType = contentType,
            status = status.toFileItemStatus(progress = progress),
            timeCategory = categorizeGroup(
                epochMillis = createdTime,
                status = status,
            ),
            description = fileItemDescriptionProvider.getDescription(downloadState = this),
        )

    private fun DownloadState.Status.toFileItemStatus(
        @FloatRange(from = 0.0, to = 1.0) progress: Float?,
    ): FileItem.Status = when (this) {
        DownloadState.Status.INITIATED -> FileItem.Status.Initiated
        DownloadState.Status.DOWNLOADING -> FileItem.Status.Downloading(progress = progress)
        DownloadState.Status.PAUSED -> FileItem.Status.Paused(progress = progress)
        DownloadState.Status.CANCELLED -> FileItem.Status.Cancelled
        DownloadState.Status.FAILED -> FileItem.Status.Failed
        DownloadState.Status.COMPLETED -> FileItem.Status.Completed
    }

    private fun categorizeGroup(epochMillis: Long, status: DownloadState.Status): TimeCategory {
        if (isDisplayableItem(status) && status != DownloadState.Status.COMPLETED) {
            return TimeCategory.IN_PROGRESS
        }

        val currentDate = dateTimeProvider.currentLocalDate()
        val inputDate = Instant.ofEpochMilli(epochMillis)
            .atZone(dateTimeProvider.currentZoneId())
            .toLocalDate()

        return when {
            inputDate.isEqual(currentDate) -> TimeCategory.TODAY
            inputDate.isEqual(currentDate.minusDays(1)) -> TimeCategory.YESTERDAY
            inputDate.isAfter(currentDate.minusDays(NUM_DAYS_IN_LAST_7_DAYS_PERIOD)) -> TimeCategory.LAST_7_DAYS
            inputDate.isAfter(currentDate.minusDays(NUM_DAYS_IN_LAST_30_DAYS_PERIOD)) -> TimeCategory.LAST_30_DAYS
            else -> TimeCategory.OLDER
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
