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
                id = "8",
                createdTime = CreatedTime.LAST_7_DAYS,
            ),
            fileItem(
                id = "7",
                createdTime = CreatedTime.TODAY,
            ),
        )

        val downloadUIState = DownloadUIState(
            items = fileItems,
            mode = DownloadUIState.Mode.Normal,
            pendingDeletionIds = setOf("7"),
            isDeletingItems = false,
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
}
