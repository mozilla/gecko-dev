/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen.store

import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.R

class FileItemToIconMapperTest {
    @Test
    fun getIcon() {
        val fileItem = FileItem(
            id = "0",
            url = "url",
            fileName = "MyAwesomeFile",
            filePath = "",
            description = "Test description",
            displayedShortUrl = "url",
            contentType = "image/png",
            status = FileItem.Status.Completed,
            timeCategory = TimeCategory.TODAY,
        )

        assertEquals(R.drawable.ic_file_type_image, fileItem.getIcon())
        assertEquals(R.drawable.ic_file_type_audio_note, fileItem.copy(contentType = "audio/mp3").getIcon())
        assertEquals(R.drawable.ic_file_type_video, fileItem.copy(contentType = "video/mp4").getIcon())
        assertEquals(R.drawable.ic_file_type_document, fileItem.copy(contentType = "text/csv").getIcon())
        assertEquals(R.drawable.ic_file_type_zip, fileItem.copy(contentType = "application/gzip").getIcon())
        assertEquals(R.drawable.ic_file_type_apk, fileItem.copy(contentType = null, fileName = "Fenix.apk").getIcon())
        assertEquals(R.drawable.ic_file_type_zip, fileItem.copy(contentType = null, fileName = "Fenix.zip").getIcon())
        assertEquals(R.drawable.ic_file_type_document, fileItem.copy(contentType = null, fileName = "Fenix.pdf").getIcon())
        assertEquals(R.drawable.ic_file_type_default, fileItem.copy(contentType = null, fileName = null).getIcon())
    }
}
