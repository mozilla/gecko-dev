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
            userSelectedContentTypeFilter = FileItem.ContentTypeFilter.All,
        )

        val expected = DownloadUIState.ItemsState.Items(
            listOf(
                HeaderItem(createdTime = CreatedTime.LAST_30_DAYS),
                fileItem(id = "1"),
                fileItem(id = "3"),
            ),
        )

        assertEquals(expected, downloadUIState.itemsState)
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
            userSelectedContentTypeFilter = FileItem.ContentTypeFilter.All,
        )

        val expected = DownloadUIState.ItemsState.Items(
            listOf(
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
            ),
        )

        assertEquals(expected, downloadUIState.itemsState)
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
            userSelectedContentTypeFilter = FileItem.ContentTypeFilter.Image,
        )

        val expected = DownloadUIState.ItemsState.Items(
            listOf(
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
            ),
        )

        assertEquals(expected, downloadUIState.itemsState)
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
            userSelectedContentTypeFilter = FileItem.ContentTypeFilter.Image,
        )

        val expected = FileItem.ContentTypeFilter.All

        assertEquals(expected, downloadUIState.selectedContentTypeFilter)
    }

    @Test
    fun `WHEN search query is used THEN only the items matching query for url are displayed`() {
        val fileItems = listOf(
            fileItem(
                id = "1",
                fileName = "somefile",
                displayedShortUrl = "firefox.com",
                createdTime = CreatedTime.LAST_30_DAYS,
            ),
            fileItem(
                id = "2",
                fileName = "anotherfile",
                displayedShortUrl = "mozilla.org",
                createdTime = CreatedTime.LAST_30_DAYS,
            ),
            fileItem(
                id = "3",
                fileName = "yetanotherfile",
                displayedShortUrl = "mozilla.com",
                createdTime = CreatedTime.LAST_30_DAYS,
            ),
            fileItem(
                id = "4",
                fileName = "name",
                displayedShortUrl = "firefox.com",
                createdTime = CreatedTime.LAST_30_DAYS,
            ),
        )

        val downloadUIState = DownloadUIState(
            items = fileItems,
            mode = DownloadUIState.Mode.Normal,
            pendingDeletionIds = emptySet(),
            searchQuery = "firefox",
        )

        val expected = DownloadUIState.ItemsState.Items(
            listOf(
                HeaderItem(createdTime = CreatedTime.LAST_30_DAYS),
                fileItem(
                    id = "1",
                    fileName = "somefile",
                    displayedShortUrl = "firefox.com",
                    createdTime = CreatedTime.LAST_30_DAYS,
                ),
                fileItem(
                    id = "4",
                    fileName = "name",
                    displayedShortUrl = "firefox.com",
                    createdTime = CreatedTime.LAST_30_DAYS,
                ),
            ),
        )

        assertEquals(expected, downloadUIState.itemsState)
    }

    @Test
    fun `WHEN search query is used THEN only the items matching query for name are displayed`() {
        val fileItems = listOf(
            fileItem(
                id = "1",
                fileName = "somefile",
                displayedShortUrl = "firefox.com",
                createdTime = CreatedTime.LAST_30_DAYS,
            ),
            fileItem(
                id = "2",
                fileName = "anotherfile",
                displayedShortUrl = "mozilla.org",
                createdTime = CreatedTime.LAST_30_DAYS,
            ),
            fileItem(
                id = "3",
                fileName = "yetanotherfile",
                displayedShortUrl = "mozilla.com",
                createdTime = CreatedTime.LAST_30_DAYS,
            ),
            fileItem(
                id = "4",
                fileName = "name",
                displayedShortUrl = "firefox.com",
                createdTime = CreatedTime.LAST_30_DAYS,
            ),
        )

        val downloadUIState = DownloadUIState(
            items = fileItems,
            mode = DownloadUIState.Mode.Normal,
            pendingDeletionIds = emptySet(),
            searchQuery = "file",
        )

        val expected = DownloadUIState.ItemsState.Items(
            listOf(
                HeaderItem(createdTime = CreatedTime.LAST_30_DAYS),
                fileItem(
                    id = "1",
                    fileName = "somefile",
                    displayedShortUrl = "firefox.com",
                    createdTime = CreatedTime.LAST_30_DAYS,
                ),
                fileItem(
                    id = "2",
                    fileName = "anotherfile",
                    displayedShortUrl = "mozilla.org",
                    createdTime = CreatedTime.LAST_30_DAYS,
                ),
                fileItem(
                    id = "3",
                    fileName = "yetanotherfile",
                    displayedShortUrl = "mozilla.com",
                    createdTime = CreatedTime.LAST_30_DAYS,
                ),
            ),
        )

        assertEquals(expected, downloadUIState.itemsState)
    }

    @Test
    fun `WHEN search query has no matches THEN no results ui is displayed`() {
        val fileItems = listOf(
            fileItem(
                id = "1",
                fileName = "somefile",
                displayedShortUrl = "firefox.com",
                createdTime = CreatedTime.LAST_30_DAYS,
            ),
            fileItem(
                id = "2",
                fileName = "anotherfile",
                displayedShortUrl = "mozilla.org",
                createdTime = CreatedTime.LAST_30_DAYS,
            ),
            fileItem(
                id = "3",
                fileName = "yetanotherfile",
                displayedShortUrl = "mozilla.com",
                createdTime = CreatedTime.LAST_30_DAYS,
            ),
            fileItem(
                id = "4",
                fileName = "name",
                displayedShortUrl = "firefox.com",
                createdTime = CreatedTime.LAST_30_DAYS,
            ),
        )

        val downloadUIState = DownloadUIState(
            items = fileItems,
            mode = DownloadUIState.Mode.Normal,
            pendingDeletionIds = emptySet(),
            searchQuery = "blablah",
        )

        val expected = DownloadUIState.ItemsState.NoSearchResults

        assertEquals(expected, downloadUIState.itemsState)
    }

    @Test
    fun `WHEN there are no files THEN empty state ui is displayed`() {
        val downloadUIState = DownloadUIState(
            items = emptyList(),
            mode = DownloadUIState.Mode.Normal,
            pendingDeletionIds = emptySet(),
        )

        val expected = DownloadUIState.ItemsState.NoItems

        assertEquals(expected, downloadUIState.itemsState)
    }

    @Test
    fun `WHEN there are not items THEN search icon is not visible`() {
        val downloadUIState = DownloadUIState(
            items = emptyList(),
            mode = DownloadUIState.Mode.Normal,
            pendingDeletionIds = emptySet(),
            isSearchFieldRequested = false,
            searchQuery = "",
        )

        assertEquals(false, downloadUIState.isSearchIconVisible)
    }

    @Test
    fun `WHEN state is in edit mode THEN search icon is not visible`() {
        val fileItem1 = fileItem(
            id = "1",
            fileName = "somefile",
            displayedShortUrl = "firefox.com",
            createdTime = CreatedTime.LAST_30_DAYS,
        )
        val fileItem2 = fileItem(
            id = "2",
            fileName = "anotherfile",
            displayedShortUrl = "mozilla.org",
            createdTime = CreatedTime.LAST_30_DAYS,
        )
        val fileItems = listOf(
            fileItem1,
            fileItem2,
        )

        val downloadUIState = DownloadUIState(
            items = fileItems,
            mode = DownloadUIState.Mode.Editing(
                selectedItems = setOf(fileItem1),
            ),
            pendingDeletionIds = emptySet(),
            isSearchFieldRequested = true,
            searchQuery = "",
        )

        assertEquals(false, downloadUIState.isSearchIconVisible)
    }

    @Test
    fun `WHEN search field is visible THEN search icon is not visible`() {
        val fileItem1 = fileItem(
            id = "1",
            fileName = "somefile",
            displayedShortUrl = "firefox.com",
            createdTime = CreatedTime.LAST_30_DAYS,
        )
        val fileItem2 = fileItem(
            id = "2",
            fileName = "anotherfile",
            displayedShortUrl = "mozilla.org",
            createdTime = CreatedTime.LAST_30_DAYS,
        )
        val fileItems = listOf(
            fileItem1,
            fileItem2,
        )

        val downloadUIState = DownloadUIState(
            items = fileItems,
            mode = DownloadUIState.Mode.Normal,
            pendingDeletionIds = emptySet(),
            isSearchFieldRequested = true,
            searchQuery = "",
        )

        assertEquals(false, downloadUIState.isSearchIconVisible)
    }

    @Test
    fun `WHEN search field is not requested and state is in normal mode with items THEN search icon is visible`() {
        val fileItem1 = fileItem(
            id = "1",
            fileName = "somefile",
            displayedShortUrl = "firefox.com",
            createdTime = CreatedTime.LAST_30_DAYS,
        )
        val fileItem2 = fileItem(
            id = "2",
            fileName = "anotherfile",
            displayedShortUrl = "mozilla.org",
            createdTime = CreatedTime.LAST_30_DAYS,
        )
        val fileItems = listOf(
            fileItem1,
            fileItem2,
        )

        val downloadUIState = DownloadUIState(
            items = fileItems,
            mode = DownloadUIState.Mode.Normal,
            pendingDeletionIds = emptySet(),
            isSearchFieldRequested = false,
            searchQuery = "",
        )

        assertEquals(true, downloadUIState.isSearchIconVisible)
    }

    @Test
    fun `WHEN search field is requested and state is in normal mode THEN search field is visible`() {
        val downloadUIState = DownloadUIState(
            items = emptyList(),
            mode = DownloadUIState.Mode.Normal,
            pendingDeletionIds = emptySet(),
            isSearchFieldRequested = true,
            searchQuery = "",
        )

        assertEquals(true, downloadUIState.isSearchFieldVisible)
    }

    @Test
    fun `WHEN search field is requested and state is in edit mode THEN search field is not visible`() {
        val downloadUIState = DownloadUIState(
            items = emptyList(),
            mode = DownloadUIState.Mode.Editing(
                selectedItems = emptySet(),
            ),
            pendingDeletionIds = emptySet(),
            isSearchFieldRequested = true,
            searchQuery = "",
        )

        assertEquals(false, downloadUIState.isSearchFieldVisible)
    }

    @Test
    fun `WHEN state is in edit mode THEN back handler is enabled`() {
        val downloadUIState = DownloadUIState(
            items = emptyList(),
            mode = DownloadUIState.Mode.Editing(
                selectedItems = emptySet(),
            ),
            pendingDeletionIds = emptySet(),
            isSearchFieldRequested = false,
            searchQuery = "",
        )

        assertEquals(true, downloadUIState.isBackHandlerEnabled)
    }

    @Test
    fun `WHEN search is visible THEN back handler is enabled`() {
        val downloadUIState = DownloadUIState(
            items = emptyList(),
            mode = DownloadUIState.Mode.Normal,
            pendingDeletionIds = emptySet(),
            isSearchFieldRequested = true,
            searchQuery = "",
        )
        // Making sure the test condition are met
        assertEquals(true, downloadUIState.isSearchFieldVisible)

        // Asserting the expected state
        assertEquals(true, downloadUIState.isBackHandlerEnabled)
    }

    @Test
    fun `WHEN state is in normal mode and search bar is not visible THEN back handler is not enabled`() {
        val downloadUIState = DownloadUIState(
            items = emptyList(),
            mode = DownloadUIState.Mode.Normal,
            pendingDeletionIds = emptySet(),
            isSearchFieldRequested = false,
            searchQuery = "",
        )

        assertEquals(false, downloadUIState.isBackHandlerEnabled)
    }
}
