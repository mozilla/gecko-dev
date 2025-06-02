/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads.fake

import mozilla.components.feature.downloads.FileSizeFormatter

/**
 * A fake file size formatter to be used for testing.
 */
class FakeFileSizeFormatter : FileSizeFormatter {
    override fun formatSizeInBytes(sizeInBytes: Long): String =
        sizeInBytes.toString()
}
