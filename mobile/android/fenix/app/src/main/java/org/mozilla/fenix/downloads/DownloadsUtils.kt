/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads

import android.content.Context
import android.webkit.MimeTypeMap
import mozilla.components.browser.state.state.content.DownloadState
import org.mozilla.fenix.R

/**
 * Generates a user-facing error message indicating that a downloaded file cannot be opened
 * because no application is available to handle its file type.
 *
 * @param context The Context used to access string resources.
 * @param download The [DownloadState] object representing the downloaded file.
 * @return A formatted string message. For example, "No app found to open .pdf files".
 */
fun getCannotOpenFileErrorMessage(context: Context, download: DownloadState): String {
    val fileExt = MimeTypeMap.getFileExtensionFromUrl(download.filePath)
    return context.getString(
        R.string.mozac_feature_downloads_open_not_supported1,
        fileExt,
    )
}
