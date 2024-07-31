/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen

import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.withContext
import mozilla.components.browser.state.state.content.DownloadState
import java.io.File

/**
 * Class representing a downloads entry
 *
 * @property id Unique id of the download item
 * @property url The full url to the content that should be downloaded
 * @property fileName File name of the download item
 * @property filePath Full path of the download item
 * @property formattedSize The formatted size of the download item
 * @property contentType The type of file the download is
 * @property status The status that represents every state that a download can be in
 */
data class DownloadItem(
    val id: String,
    val url: String,
    val fileName: String?,
    val filePath: String,
    val formattedSize: String,
    val contentType: String?,
    val status: DownloadState.Status,
)

/**
 * Returns a filtered list of [DownloadItem]s containing only items that are present on the disk.
 * If a user has deleted the downloaded item it should not show on the downloaded list.
 */
suspend fun List<DownloadItem>.filterExistsOnDisk(dispatcher: CoroutineDispatcher): List<DownloadItem> =
    withContext(dispatcher) {
        filter {
            File(it.filePath).exists()
        }
    }
