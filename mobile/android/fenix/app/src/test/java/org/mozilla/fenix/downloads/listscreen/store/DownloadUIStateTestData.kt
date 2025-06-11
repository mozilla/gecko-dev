/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen.store

fun fileItem(
    id: String = "1",
    url: String = "https://www.mozilla.org/file1",
    fileName: String? = "file1",
    filePath: String = "path1",
    description: String = "1 MB • mozilla.org",
    displayedShortUrl: String = "mozilla.org",
    contentType: String? = "image/png",
    status: FileItem.Status = FileItem.Status.Completed,
    timeCategory: TimeCategory = TimeCategory.LAST_30_DAYS,
) = FileItem(
    id = id,
    url = url,
    fileName = fileName,
    filePath = filePath,
    description = description,
    displayedShortUrl = displayedShortUrl,
    contentType = contentType,
    status = status,
    timeCategory = timeCategory,
)
