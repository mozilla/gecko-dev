/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen.store

import org.junit.Assert.assertEquals
import org.junit.Test

class DownloadUIStateTest {

    @Test
    fun `items pending deletion are not displayed`() {
        val fileItems = listOf(
            fileItem(
                id = "1",
                createdTime = CreatedTime.LAST_30_DAYS,
            ),
            fileItem(
                id = "2",
                createdTime = CreatedTime.LAST_30_DAYS,
            ),
            fileItem(
                id = "3",
                createdTime = CreatedTime.LAST_30_DAYS,
            ),
            fileItem(
                id = "4",
                createdTime = CreatedTime.LAST_30_DAYS,
            ),
        )

        val downloadUIState = DownloadUIState(
            items = fileItems,
            mode = DownloadUIState.Mode.Normal,
            pendingDeletionIds = setOf("2", "4"),
            isDeletingItems = false,
            userSelectedContentTypeFilter = FileItem.ContentTypeFilter.All,
        )

        val expected = listOf(
            HeaderItem(createdTime = CreatedTime.LAST_30_DAYS),
            fileItem(id = "1"),
            fileItem(id = "3"),
        )

        assertEquals(expected, downloadUIState.itemsToDisplay)
    }

    @Test
    fun `items are grouped by time period`() {
        val fileItems = listOf(
            fileItem(
                id = "1",
                createdTime = CreatedTime.OLDER,
            ),
            fileItem(
                id = "2",
                createdTime = CreatedTime.OLDER,
            ),
            fileItem(
                id = "3",
                createdTime = CreatedTime.LAST_30_DAYS,
            ),
            fileItem(
                id = "4",
                createdTime = CreatedTime.LAST_30_DAYS,
            ),
            fileItem(
                id = "5",
                createdTime = CreatedTime.YESTERDAY,
            ),
            fileItem(
                id = "6",
                createdTime = CreatedTime.TODAY,
            ),
            fileItem(
                id = "7",
                createdTime = CreatedTime.TODAY,
            ),
            fileItem(
                id = "8",
                createdTime = CreatedTime.LAST_7_DAYS,
            ),
        )

        val downloadUIState = DownloadUIState(
            items = fileItems,
            mode = DownloadUIState.Mode.Normal,
            pendingDeletionIds = setOf("7"),
            isDeletingItems = false,
            userSelectedContentTypeFilter = FileItem.ContentTypeFilter.All,
        )

        val expected = listOf(
            HeaderItem(createdTime = CreatedTime.TODAY),
            fileItem(
                id = "6",
                createdTime = CreatedTime.TODAY,
            ),
            HeaderItem(createdTime = CreatedTime.YESTERDAY),
            fileItem(
                id = "5",
                createdTime = CreatedTime.YESTERDAY,
            ),
            HeaderItem(createdTime = CreatedTime.LAST_7_DAYS),
            fileItem(
                id = "8",
                createdTime = CreatedTime.LAST_7_DAYS,
            ),
            HeaderItem(createdTime = CreatedTime.LAST_30_DAYS),
            fileItem(
                id = "3",
                createdTime = CreatedTime.LAST_30_DAYS,
            ),
            fileItem(
                id = "4",
                createdTime = CreatedTime.LAST_30_DAYS,
            ),
            HeaderItem(createdTime = CreatedTime.OLDER),
            fileItem(
                id = "1",
                createdTime = CreatedTime.OLDER,
            ),
            fileItem(
                id = "2",
                createdTime = CreatedTime.OLDER,
            ),
        )

        assertEquals(expected, downloadUIState.itemsToDisplay)
    }

    @Test
    fun `WHEN content type filter is selected THEN only those items are displayed`() {
        val fileItems = listOf(
            fileItem(
                id = "1",
                createdTime = CreatedTime.OLDER,
                contentType = "image/png",
            ),
            fileItem(
                id = "2",
                createdTime = CreatedTime.OLDER,
                contentType = "video/mp4",
            ),
            fileItem(
                id = "3",
                createdTime = CreatedTime.LAST_30_DAYS,
                contentType = "application/pdf",
            ),
            fileItem(
                id = "4",
                createdTime = CreatedTime.LAST_30_DAYS,
                contentType = "text/plain",
            ),
            fileItem(
                id = "5",
                createdTime = CreatedTime.YESTERDAY,
                contentType = "image/png",
            ),
            fileItem(
                id = "6",
                createdTime = CreatedTime.TODAY,
                contentType = "image/png",
            ),
            fileItem(
                id = "7",
                createdTime = CreatedTime.TODAY,
                contentType = "image/png",
            ),
        )

        val downloadUIState = DownloadUIState(
            items = fileItems,
            mode = DownloadUIState.Mode.Normal,
            pendingDeletionIds = emptySet(),
            isDeletingItems = false,
            userSelectedContentTypeFilter = FileItem.ContentTypeFilter.Image,
        )

        val expected = listOf(
            HeaderItem(createdTime = CreatedTime.TODAY),
            fileItem(
                id = "6",
                createdTime = CreatedTime.TODAY,
                contentType = "image/png",
            ),
            fileItem(
                id = "7",
                createdTime = CreatedTime.TODAY,
                contentType = "image/png",
            ),
            HeaderItem(createdTime = CreatedTime.YESTERDAY),
            fileItem(
                id = "5",
                createdTime = CreatedTime.YESTERDAY,
                contentType = "image/png",
            ),
            HeaderItem(createdTime = CreatedTime.OLDER),
            fileItem(
                id = "1",
                createdTime = CreatedTime.OLDER,
                contentType = "image/png",
            ),
        )

        assertEquals(expected, downloadUIState.itemsToDisplay)
        assertEquals(FileItem.ContentTypeFilter.Image, downloadUIState.selectedContentTypeFilter)
    }

    @Test
    fun `WHEN file items of at least 2 types are present THEN only those filters for available file types`() {
        val fileItems = listOf(
            fileItem(
                id = "2",
                createdTime = CreatedTime.OLDER,
                contentType = "video/mp4",
            ),
            fileItem(
                id = "3",
                createdTime = CreatedTime.LAST_30_DAYS,
                contentType = "application/pdf",
            ),
            fileItem(
                id = "4",
                createdTime = CreatedTime.LAST_30_DAYS,
                contentType = "text/plain",
            ),
        )

        val downloadUIState = DownloadUIState(
            items = fileItems,
            mode = DownloadUIState.Mode.Normal,
            pendingDeletionIds = emptySet(),
            isDeletingItems = false,
            filtersFeatureFlag = true,
            userSelectedContentTypeFilter = FileItem.ContentTypeFilter.All,
        )

        val expected = listOf(
            FileItem.ContentTypeFilter.All, // All is present when there are items
            FileItem.ContentTypeFilter.Video,
            FileItem.ContentTypeFilter.Document,
        )

        assertEquals(expected, downloadUIState.filtersToDisplay)
    }

    @Test
    fun `WHEN file items of only one type are present THEN filters are not displayed`() {
        val fileItems = listOf(
            fileItem(
                id = "2",
                createdTime = CreatedTime.OLDER,
                contentType = "video/mp4",
            ),
            fileItem(
                id = "3",
                createdTime = CreatedTime.LAST_30_DAYS,
                contentType = "video/mp4",
            ),
        )

        val downloadUIState = DownloadUIState(
            items = fileItems,
            mode = DownloadUIState.Mode.Normal,
            pendingDeletionIds = emptySet(),
            isDeletingItems = false,
            filtersFeatureFlag = true,
            userSelectedContentTypeFilter = FileItem.ContentTypeFilter.All,
        )

        val expected = emptyList<FileItem.ContentTypeFilter>()

        assertEquals(expected, downloadUIState.filtersToDisplay)
    }

    @Test
    fun `WHEN filters feature flag is off THEN filters are not displayed`() {
        val fileItems = listOf(
            fileItem(
                id = "2",
                createdTime = CreatedTime.OLDER,
                contentType = "video/mp4",
            ),
            fileItem(
                id = "3",
                createdTime = CreatedTime.LAST_30_DAYS,
                contentType = "application/pdf",
            ),
            fileItem(
                id = "4",
                createdTime = CreatedTime.LAST_30_DAYS,
                contentType = "text/plain",
            ),
        )

        val downloadUIState = DownloadUIState(
            items = fileItems,
            mode = DownloadUIState.Mode.Normal,
            pendingDeletionIds = emptySet(),
            isDeletingItems = false,
            filtersFeatureFlag = false,
            userSelectedContentTypeFilter = FileItem.ContentTypeFilter.All,
        )

        val expected = emptyList<FileItem.ContentTypeFilter>()

        assertEquals(expected, downloadUIState.filtersToDisplay)
    }

    @Test
    fun `WHEN user selected filter does not contain any items THEN override filter is set to All`() {
        val fileItems = listOf(
            fileItem(
                id = "2",
                createdTime = CreatedTime.OLDER,
                contentType = "video/mp4",
            ),
            fileItem(
                id = "3",
                createdTime = CreatedTime.LAST_30_DAYS,
                contentType = "application/pdf",
            ),
            fileItem(
                id = "4",
                createdTime = CreatedTime.LAST_30_DAYS,
                contentType = "text/plain",
            ),
        )

        val downloadUIState = DownloadUIState(
            items = fileItems,
            mode = DownloadUIState.Mode.Normal,
            pendingDeletionIds = emptySet(),
            isDeletingItems = false,
            filtersFeatureFlag = false,
            userSelectedContentTypeFilter = FileItem.ContentTypeFilter.Image,
        )

        val expected = FileItem.ContentTypeFilter.All

        assertEquals(expected, downloadUIState.selectedContentTypeFilter)
    }
}
