/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen.store

import androidx.annotation.DrawableRes
import androidx.annotation.FloatRange
import androidx.annotation.StringRes
import mozilla.components.browser.state.state.content.DownloadState
import org.mozilla.fenix.R

/**
 * Sealed interface representing a download list item
 */
sealed interface DownloadListItem

/**
 * Class representing a downloaded file item
 *
 * @property id Unique id of the download item
 * @property url The full url to the content that should be downloaded
 * @property fileName File name of the download item
 * @property filePath Full path of the download item
 * @property displayedShortUrl The shortened url of the download item
 * @property contentType The type of file the download is
 * @property status The download status of the item
 * @property timeCategory The time period the file was downloaded in
 * @property description The description of the file item on the downloads screen
 */
data class FileItem(
    val id: String,
    val url: String,
    val fileName: String?,
    val filePath: String,
    val displayedShortUrl: String,
    val contentType: String?,
    val status: Status,
    val timeCategory: TimeCategory,
    val description: String,
) : DownloadListItem {

    /**
     * The icon resource ID associated with this [FileItem].
     */
    @DrawableRes
    val icon: Int = getIcon()

    /**
     * The content type filter based on the [contentType] of the [FileItem]
     */
    val matchingContentTypeFilter: ContentTypeFilter
        get() = (ContentTypeFilter.interestingContentTypes)
            .first { type -> type.predicate(contentType) }

    /**
     * Text against which the search query is matched.
     */
    val stringToMatchForSearchQuery: String
        get() = "$fileName $displayedShortUrl"

    /**
     * Enum class representing the content type filter options
     *
     * @property stringRes The string resource id of the content type filter
     * @property predicate The predicate for the content type filter
     */
    enum class ContentTypeFilter(
        @StringRes val stringRes: Int,
        val predicate: (String?) -> Boolean,
    ) {
        All(
            stringRes = R.string.download_content_type_filter_all,
            predicate = { true },
        ),
        Image(
            stringRes = R.string.download_content_type_filter_image,
            predicate = { it?.startsWith("image/") == true },
        ),
        Video(
            stringRes = R.string.download_content_type_filter_video,
            predicate = { it?.startsWith("video/") == true },
        ),
        Document(
            stringRes = R.string.download_content_type_filter_document,
            predicate = {
                // We extract MIME types corresponding to documents from the list of the most common
                // MIME types from MDN.
                // https://developer.mozilla.org/en-US/docs/Web/HTTP/Guides/MIME_types/Common_types
                it?.startsWith("text/") == true || it in listOf(
                    "application/vnd.ms-excel",
                    "application/msword",
                    "application/vnd.ms-powerpoint",
                    "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
                    "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
                    "application/vnd.openxmlformats-officedocument.presentationml.presentation",
                    "application/vnd.oasis.opendocument.text",
                    "application/vnd.oasis.opendocument.spreadsheet",
                    "application/vnd.oasis.opendocument.presentation",
                    "application/pdf",
                    "application/rtf",
                    "application/epub+zip",
                    "application/vnd.amazon.ebook",
                    "application/xml",
                    "application/json",
                    "application/vnd.apple.keynote",
                    "application/x-abiword",
                )
            },
        ),
        Other(
            stringRes = R.string.download_content_type_filter_other_1,
            predicate = { !Image.predicate(it) && !Video.predicate(it) && !Document.predicate(it) },
        ),
        ;

        /**
         * @see [ContentTypeFilter].
         */
        companion object {
            val interestingContentTypes = entries - All
        }
    }

    /**
     * The download status of the item.
     */
    sealed interface Status {

        /**
         * Transitions the status to the next state based on the [action].
         */
        fun transition(action: DownloadControlAction): Status

        /**
         * Enum class representing the download actions that a user can trigger on a [FileItem].
         */
        enum class DownloadControlAction {
            PAUSE,
            RESUME,
            RETRY,
            CANCEL,
        }

        /**
         * Indicates that the download is in the first state after creation but not yet [Downloading].
         */
        data object Initiated : Status {
            override fun transition(action: DownloadControlAction): Status = when (action) {
                DownloadControlAction.CANCEL -> Cancelled
                else -> this
            }
        }

        /**
         * Indicates that an [Initiated] download is now actively being downloaded.
         */
        data class Downloading(
            @FloatRange(from = 0.0, to = 1.0) val progress: Float?,
        ) : Status {
            override fun transition(action: DownloadControlAction): Status = when (action) {
                DownloadControlAction.PAUSE -> Paused(progress = progress)
                DownloadControlAction.CANCEL -> Cancelled
                else -> this
            }
        }

        /**
         * Indicates that the download that has been [Downloading] has been paused.
         */
        data class Paused(
            @FloatRange(from = 0.0, to = 1.0) val progress: Float?,
        ) : Status {
            override fun transition(action: DownloadControlAction): Status = when (action) {
                DownloadControlAction.RESUME -> Downloading(progress = progress)
                DownloadControlAction.CANCEL -> Cancelled
                else -> this
            }
        }

        /**
         * Indicates that the download that has been [Downloading] has been cancelled.
         */
        data object Cancelled : Status {
            override fun transition(action: DownloadControlAction): Status = this
        }

        /**
         * Indicates that the download that has been [Downloading] has moved to failed because
         * something unexpected has happened.
         */
        data object Failed : Status {
            override fun transition(action: DownloadControlAction): Status = when (action) {
                DownloadControlAction.RETRY -> Initiated
                DownloadControlAction.CANCEL -> Cancelled
                else -> this
            }
        }

        /**
         * Indicates that the [Downloading] download has been completed.
         */
        data object Completed : Status {
            override fun transition(action: DownloadControlAction): Status = this
        }

        /**
         * Convert a [Status] to a [DownloadState.Status].
         */
        fun toDownloadStateStatus(): DownloadState.Status = when (this) {
            is Initiated -> DownloadState.Status.INITIATED
            is Downloading -> DownloadState.Status.DOWNLOADING
            is Paused -> DownloadState.Status.PAUSED
            is Cancelled -> DownloadState.Status.CANCELLED
            is Failed -> DownloadState.Status.FAILED
            is Completed -> DownloadState.Status.COMPLETED
        }
    }
}

/**
 * Class representing a downloads section header
 *
 * @property timeCategory The time period the header represents
 */
data class HeaderItem(
    val timeCategory: TimeCategory,
) : DownloadListItem

/**
 * Enum class representing the time period used to group download items
 */
enum class TimeCategory(
    @StringRes val stringRes: Int,
) {
    /**
     * Represents a download that is in progress
     */
    IN_PROGRESS(R.string.download_header_in_progress),

    /**
     * Represents the current day
     */
    TODAY(R.string.download_time_period_today),

    /**
     * Represents the day before the current day
     */
    YESTERDAY(R.string.download_time_period_yesterday),

    /**
     * Represents the last 7 days
     */
    LAST_7_DAYS(R.string.download_time_period_last_7_days),

    /**
     * Represents the last 30 days
     */
    LAST_30_DAYS(R.string.download_time_period_last_30_days),

    /**
     * Represents a time period older than 30 days
     */
    OLDER(R.string.download_time_period_older),
}
