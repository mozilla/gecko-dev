/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen.store

import mozilla.components.browser.state.state.content.DownloadState
import org.junit.Assert.assertEquals
import org.junit.Test

class FileItemTest {

    @Test
    fun `WHEN file item has a non-document content type THEN it is recognized by the correct ContentTypeFilter`() {
        val image = fileItem(contentType = "image/png")
        val video = fileItem(contentType = "video/mp4")
        val other = fileItem(contentType = "application/zip")
        val noContentType = fileItem(contentType = null)

        assertEquals(FileItem.ContentTypeFilter.Image, image.matchingContentTypeFilter)
        assertEquals(FileItem.ContentTypeFilter.Video, video.matchingContentTypeFilter)
        assertEquals(FileItem.ContentTypeFilter.Other, other.matchingContentTypeFilter)
        assertEquals(FileItem.ContentTypeFilter.Other, noContentType.matchingContentTypeFilter)
    }

    @Test
    fun `WHEN file item is in progress THEN it is recognized by the correct ContentTypeFilter`() {
        val inProgressFile = fileItem(
            contentType = "image/png",
            status = DownloadState.Status.DOWNLOADING,
        )
        assertEquals(FileItem.ContentTypeFilter.Image, inProgressFile.matchingContentTypeFilter)
    }

    @Test
    fun `WHEN file item is paused THEN it should only match the All ContentTypeFilter`() {
        val pausedFile = fileItem(
            contentType = "image/png",
            status = DownloadState.Status.PAUSED,
        )
        assertEquals(FileItem.ContentTypeFilter.Image, pausedFile.matchingContentTypeFilter)
    }

    @Test
    fun `WHEN file item failed to download THEN it should only match the All ContentTypeFilter`() {
        val failedFile = fileItem(
            contentType = "image/png",
            status = DownloadState.Status.FAILED,
        )
        assertEquals(FileItem.ContentTypeFilter.Image, failedFile.matchingContentTypeFilter)
    }

    @Test
    fun `WHEN file's mimetype correspond to a document THEN the Document contentTypeFilter is returned`() {
        val documentMimeTypes = listOf(
            "application/vnd.ms-excel",
            "application/msword",
            "application/vnd.ms-powerpoint",
            "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
            "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
            "application/vnd.openxmlformats-officedocument.presentationml.presentation",
            "application/vnd.oasis.opendocument.text",
            "application/vnd.oasis.opendocument.spreadsheet",
            "application/vnd.oasis.opendocument.presentation",
            "application/pdf",
            "application/rtf",
            "application/epub+zip",
            "application/vnd.amazon.ebook",
            "application/xml",
            "application/json",
            "application/vnd.apple.keynote",
            "application/x-abiword",
        )

        for (mimeType in documentMimeTypes) {
            val fileItem = fileItem(contentType = mimeType)
            assertEquals(
                "MIME type $mimeType should be classified as Document",
                FileItem.ContentTypeFilter.Document,
                fileItem.matchingContentTypeFilter,
            )
        }
    }
}
