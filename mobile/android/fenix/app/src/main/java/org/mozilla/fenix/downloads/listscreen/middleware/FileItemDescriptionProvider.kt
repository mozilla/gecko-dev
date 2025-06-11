/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen.middleware

import android.content.Context
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.feature.downloads.DownloadEstimator
import mozilla.components.feature.downloads.FileSizeFormatter
import org.mozilla.fenix.R
import org.mozilla.fenix.downloads.listscreen.store.FileItem
import org.mozilla.fenix.ext.getBaseDomainUrl
import kotlin.time.Duration.Companion.seconds

/**
 * Interface for providing the description of the [FileItem]s on the downloads screen.
 */
interface FileItemDescriptionProvider {

    /**
     * Get the description of the [FileItem].
     *
     * @param downloadState The download state of the file item.
     */
    fun getDescription(
        downloadState: DownloadState,
    ): String
}

/**
 * The default implementation of [FileItemDescriptionProvider].
 *
 * @param context The Android context used to retrieve string resources and retrieve the [DateTimeProvider].
 * @param fileSizeFormatter [FileSizeFormatter] used to format the size of the file item.
 * @param downloadEstimator [DownloadEstimator] used to estimate the download time remaining.
 */
class DefaultFileItemDescriptionProvider(
    private val context: Context,
    private val fileSizeFormatter: FileSizeFormatter,
    private val downloadEstimator: DownloadEstimator,
) : FileItemDescriptionProvider {

    override fun getDescription(
        downloadState: DownloadState,
    ): String = when (downloadState.status) {
        DownloadState.Status.COMPLETED -> {
            val formattedContentLength = fileSizeFormatter.formatSizeInBytes(downloadState.contentLength ?: 0)
            "$formattedContentLength • ${downloadState.url.getBaseDomainUrl()}"
        }
        DownloadState.Status.FAILED -> context.getString(R.string.download_item_status_failed)
        DownloadState.Status.CANCELLED -> "" // Cancelled downloads are not shown
        DownloadState.Status.PAUSED -> context.getString(
            R.string.download_item_paused_description,
            fileSizeFormatter.formatSizeInBytes(downloadState.currentBytesCopied),
            fileSizeFormatter.formatSizeInBytes(downloadState.contentLength ?: 0),
        )
        DownloadState.Status.INITIATED -> context.getString(
            R.string.download_item_in_progress_description_preparing,
        )
        DownloadState.Status.DOWNLOADING -> {
            val estimatedSecsRemaining = downloadState.contentLength?.let { contentLength ->
                downloadEstimator.estimatedRemainingTime(
                    startTime = downloadState.createdTime,
                    bytesDownloaded = downloadState.currentBytesCopied,
                    totalBytes = contentLength,
                )
            }

            if (downloadState.contentLength == null) {
                // This will be changed in https://bugzilla.mozilla.org/show_bug.cgi?id=1971338
                fileSizeFormatter.formatSizeInBytes(downloadState.currentBytesCopied)
            } else if (estimatedSecsRemaining == null) {
                context.getString(
                    R.string.download_item_in_progress_description_pending,
                    fileSizeFormatter.formatSizeInBytes(downloadState.currentBytesCopied),
                    fileSizeFormatter.formatSizeInBytes(downloadState.contentLength ?: 0),
                )
            } else {
                context.getString(
                    R.string.download_item_in_progress_description_2,
                    fileSizeFormatter.formatSizeInBytes(downloadState.currentBytesCopied),
                    fileSizeFormatter.formatSizeInBytes(downloadState.contentLength ?: 0),
                    estimatedSecsRemaining.seconds.toString(),
                )
            }
        }
    }
}
