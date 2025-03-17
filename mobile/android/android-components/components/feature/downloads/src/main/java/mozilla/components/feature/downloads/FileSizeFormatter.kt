/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.content.Context
import android.text.format.Formatter

/**
 * Utility interface to format the size of a file in a localized manner.
 */
interface FileSizeFormatter {

    /**
     * Formats a content size to be in the form of bytes, kilobytes, megabytes, and gigabytes.
     *
     * @param sizeInBytes Size value to be formatted, in bytes.
     */
    fun formatSizeInBytes(sizeInBytes: Long): String
}

/**
 * The default implementation of [FileSizeFormatter] using the Android [Formatter].
 *
 * @param context The Android [Context].
 */
class DefaultFileSizeFormatter(private val context: Context) : FileSizeFormatter {
    override fun formatSizeInBytes(sizeInBytes: Long): String =
        Formatter.formatFileSize(context, sizeInBytes)
}
