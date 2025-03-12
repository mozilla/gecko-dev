/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen.store

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
 * @property formattedSize The formatted size of the download item
 * @property contentType The type of file the download is
 * @property status The status that represents every state that a download can be in
 * @property createdTime The time period the file was downloaded in
 */
data class FileItem(
    val id: String,
    val url: String,
    val fileName: String?,
    val filePath: String,
    val formattedSize: String,
    val contentType: String?,
    val status: DownloadState.Status,
    val createdTime: CreatedTime,
) : DownloadListItem

/**
 * Class representing a downloads section header
 *
 * @property createdTime The time period the header represents
 */
data class HeaderItem(
    val createdTime: CreatedTime,
) : DownloadListItem

/**
 * Enum class representing the time period used to group download items
 */
enum class CreatedTime(
    @StringRes val stringRes: Int,
) {
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
