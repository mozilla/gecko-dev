/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen

import androidx.annotation.DrawableRes
import org.mozilla.fenix.R
import org.mozilla.fenix.downloads.listscreen.store.FileItem

/**
 * Returns the icon resource id for a file based on its content type.
 */
@DrawableRes
fun FileItem.getIcon(): Int {
    return contentType?.let { contentType ->
        getIconFromContentType(contentType)
    } ?: getIconCornerCases(fileName)
}

private fun getIconFromContentType(contentType: String): Int? {
    return when {
        contentType.contains("image/") -> R.drawable.ic_file_type_image
        contentType.contains("audio/") -> R.drawable.ic_file_type_audio_note
        contentType.contains("video/") -> R.drawable.ic_file_type_video
        contentType.contains("application/") -> checkForApplicationArchiveSubtypes(contentType)
        contentType.contains("text/") -> R.drawable.ic_file_type_document
        else -> null
    }
}

private fun checkForApplicationArchiveSubtypes(contentType: String): Int? {
    return when {
        contentType.contains("rar") -> R.drawable.ic_file_type_zip
        contentType.contains("zip") -> R.drawable.ic_file_type_zip
        contentType.contains("7z") -> R.drawable.ic_file_type_zip
        contentType.contains("tar") -> R.drawable.ic_file_type_zip
        contentType.contains("freearc") -> R.drawable.ic_file_type_zip
        contentType.contains("octet-stream") -> null
        contentType.contains("vnd.android.package-archive") -> null
        else -> R.drawable.ic_file_type_document
    }
}

private fun getIconCornerCases(fileName: String?): Int {
    return when {
        fileName?.endsWith("apk") == true -> R.drawable.ic_file_type_apk
        fileName?.endsWith("zip") == true -> R.drawable.ic_file_type_zip
        fileName?.endsWith("pdf") == true -> R.drawable.ic_file_type_document
        else -> R.drawable.ic_file_type_default
    }
}
