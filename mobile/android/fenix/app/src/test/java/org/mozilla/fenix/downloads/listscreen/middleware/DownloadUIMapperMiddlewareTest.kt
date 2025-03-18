/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen.middleware

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.downloads.fake.FakeDateTimeProvider
import org.mozilla.fenix.downloads.fake.FakeFileSizeFormatter
import org.mozilla.fenix.downloads.listscreen.store.CreatedTime
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIState
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIStore
import org.mozilla.fenix.downloads.listscreen.store.FileItem
import org.mozilla.fenix.downloads.listscreen.store.HeaderItem
import java.io.File
import java.time.LocalDate
import java.time.ZoneId

@RunWith(AndroidJUnit4::class)
class DownloadUIMapperMiddlewareTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val dispatcher = coroutinesTestRule.testDispatcher
    private val scope = coroutinesTestRule.scope

    private val fakeFormatter = FakeFileSizeFormatter()

    @Test
    fun `WHEN downloads store is initialised THEN downloads state is updated to be sorted by created time`() {
        val fakeDateTimeProvider = FakeDateTimeProvider(LocalDate.of(2025, 5, 31))
        val zoneId = fakeDateTimeProvider.currentZoneId()

        val downloads = mapOf(
            "1" to DownloadState(
                id = "1",
                createdTime = LocalDate.of(2025, 3, 1).toEpochMilli(zoneId),
                url = "url",
                fileName = "1.pdf",
                status = DownloadState.Status.COMPLETED,
                destinationDirectory = "",
                directoryPath = "downloads",
                contentType = "application/pdf",
            ),
            "2" to DownloadState(
                id = "2",
                createdTime = LocalDate.of(2025, 4, 12).toEpochMilli(zoneId),
                url = "url",
                fileName = "2.pdf",
                status = DownloadState.Status.FAILED,
                destinationDirectory = "",
                directoryPath = "",
            ),
            "3" to DownloadState(
                id = "3",
                createdTime = LocalDate.of(2025, 5, 31).toEpochMilli(zoneId),
                url = "url",
                fileName = "3.pdf",
                status = DownloadState.Status.COMPLETED,
                destinationDirectory = "",
                directoryPath = "downloads",
                contentType = "text/plain",
            ),
        )
        val browserStore = BrowserStore(initialState = BrowserState(downloads = downloads))

        File("downloads").mkdirs()
        val file1 = File("downloads/1.pdf")
        val file3 = File("downloads/3.pdf")

        // Create files
        file1.createNewFile()
        file3.createNewFile()

        val downloadsStore = DownloadUIStore(
            initialState = DownloadUIState.INITIAL,
            middleware = listOf(
                DownloadUIMapperMiddleware(
                    browserStore = browserStore,
                    fileSizeFormatter = fakeFormatter,
                    scope = scope,
                    ioDispatcher = dispatcher,
                    dateTimeProvider = fakeDateTimeProvider,
                ),
            ),
        )
        downloadsStore.waitUntilIdle()
        dispatcher.scheduler.advanceUntilIdle()
        downloadsStore.waitUntilIdle()

        val expectedList =
            listOf(
                HeaderItem(CreatedTime.TODAY),
                FileItem(
                    id = "3",
                    url = "url",
                    fileName = "3.pdf",
                    filePath = "downloads/3.pdf",
                    formattedSize = "0",
                    contentType = "text/plain",
                    status = DownloadState.Status.COMPLETED,
                    createdTime = CreatedTime.TODAY,
                ),
                HeaderItem(CreatedTime.OLDER),
                FileItem(
                    id = "1",
                    url = "url",
                    fileName = "1.pdf",
                    filePath = "downloads/1.pdf",
                    formattedSize = "0",
                    contentType = "application/pdf",
                    status = DownloadState.Status.COMPLETED,
                    createdTime = CreatedTime.OLDER,
                ),
            )

        // Cleanup files
        file1.delete()
        file3.delete()

        assertEquals(expectedList, downloadsStore.state.itemsToDisplay)
    }

    @Test
    fun `WHEN two download states point to the same existing file THEN only one download item is displayed`() {
        val downloads = mapOf(
            "1" to DownloadState(
                id = "1",
                createdTime = 1,
                url = "url",
                fileName = "1.pdf",
                status = DownloadState.Status.COMPLETED,
                contentLength = 100,
                destinationDirectory = "",
                directoryPath = "downloads",
                contentType = "application/pdf",
            ),
            "2" to DownloadState(
                id = "2",
                createdTime = 2,
                url = "url",
                fileName = "1.pdf",
                status = DownloadState.Status.COMPLETED,
                destinationDirectory = "",
                contentLength = 100,
                directoryPath = "downloads",
                contentType = "application/pdf",
            ),
        )
        val browserStore = BrowserStore(initialState = BrowserState(downloads = downloads))

        File("downloads").mkdirs()
        val file1 = File("downloads/1.pdf")
        file1.createNewFile()

        val downloadsStore = DownloadUIStore(
            initialState = DownloadUIState.INITIAL,
            middleware = listOf(
                DownloadUIMapperMiddleware(
                    browserStore = browserStore,
                    fileSizeFormatter = fakeFormatter,
                    scope = scope,
                    ioDispatcher = dispatcher,
                ),
            ),
        )
        downloadsStore.waitUntilIdle()
        dispatcher.scheduler.advanceUntilIdle()
        downloadsStore.waitUntilIdle()

        val expectedList = listOf(
            HeaderItem(CreatedTime.OLDER),
            FileItem(
                id = "1",
                url = "url",
                fileName = "1.pdf",
                filePath = "downloads/1.pdf",
                formattedSize = "100",
                contentType = "application/pdf",
                status = DownloadState.Status.COMPLETED,
                createdTime = CreatedTime.OLDER,
            ),
        )

        assertEquals(expectedList, downloadsStore.state.itemsToDisplay)

        file1.delete()
    }

    private fun LocalDate.toEpochMilli(zoneId: ZoneId): Long {
        return atStartOfDay(zoneId).toInstant().toEpochMilli()
    }
}
