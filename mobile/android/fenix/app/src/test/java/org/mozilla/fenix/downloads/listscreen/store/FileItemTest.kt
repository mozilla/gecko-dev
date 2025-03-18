/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen.store

import org.junit.Assert.assertEquals
import org.junit.Test

class FileItemTest {

    @Test
    fun `WHEN file item has a content type THEN matching content type filter is evaluated`() {
        val image = fileItem(contentType = "image/png")
        val video = fileItem(contentType = "video/mp4")
        val text = fileItem(contentType = "text/plain")
        val pdf = fileItem(contentType = "application/pdf")
        val other = fileItem(contentType = "application/zip")
        val noContentType = fileItem(contentType = null)

        assertEquals(FileItem.ContentTypeFilter.Image, image.matchingContentTypeFilter)
        assertEquals(FileItem.ContentTypeFilter.Video, video.matchingContentTypeFilter)
        assertEquals(FileItem.ContentTypeFilter.Document, text.matchingContentTypeFilter)
        assertEquals(FileItem.ContentTypeFilter.Document, pdf.matchingContentTypeFilter)
        assertEquals(FileItem.ContentTypeFilter.Other, other.matchingContentTypeFilter)
        assertEquals(FileItem.ContentTypeFilter.Other, noContentType.matchingContentTypeFilter)
    }
}
