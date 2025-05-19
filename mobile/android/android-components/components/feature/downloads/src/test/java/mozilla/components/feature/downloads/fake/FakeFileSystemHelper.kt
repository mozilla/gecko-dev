/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads.fake

import mozilla.components.feature.downloads.FileSystemHelper

class FakeFileSystemHelper(
    private val availableBitesInDirectory: Long = 0L,
    private val existingDirectories: List<String> = emptyList(),
) : FileSystemHelper {
    override fun createDirectoryIfNotExists(path: String): Boolean = true

    override fun isDirectory(path: String): Boolean = path in existingDirectories

    override fun availableBytesInDirectory(path: String): Long = availableBitesInDirectory
}
