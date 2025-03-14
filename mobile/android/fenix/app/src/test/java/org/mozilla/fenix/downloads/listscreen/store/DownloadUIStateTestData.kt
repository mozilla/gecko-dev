/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen.store

import mozilla.components.browser.state.state.content.DownloadState

fun fileItem(
    id: String = "1",
    url: String = "https://www.mozilla.org/file1",
    fileName: String? = "file1",
    filePath: String = "path1",
    formattedSize: String = "1MB",
    contentType: String? = "image/png",
    status: DownloadState.Status = DownloadState.Status.COMPLETED,
    createdTime: CreatedTime = CreatedTime.LAST_30_DAYS,
) = FileItem(
    id = id,
    url = url,
    fileName = fileName,
    filePath = filePath,
    formattedSize = formattedSize,
    contentType = contentType,
    status = status,
    createdTime = createdTime,
)
