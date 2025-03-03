/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen.store

import mozilla.components.browser.state.state.content.DownloadState

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
data class FileItem(
    val id: String,
    val url: String,
    val fileName: String?,
    val filePath: String,
    val formattedSize: String,
    val contentType: String?,
    val status: DownloadState.Status,
)
