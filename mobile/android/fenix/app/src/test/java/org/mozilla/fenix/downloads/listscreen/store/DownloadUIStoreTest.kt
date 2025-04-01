/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen.store

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotSame
import org.junit.Assert.assertSame
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.downloads.fake.FakeDateTimeProvider
import org.mozilla.fenix.downloads.fake.FakeFileSizeFormatter
import org.mozilla.fenix.downloads.listscreen.middleware.DownloadUIMapperMiddleware
import java.time.LocalDate
import java.time.ZoneId

@RunWith(AndroidJUnit4::class)
class DownloadUIStoreTest {

    @get:Rule
    val coroutineTestRule = MainCoroutineRule()
    private val scope = coroutineTestRule.scope

    private val fakeFormatter = FakeFileSizeFormatter()

    private val fileItem = FileItem(
        id = "0",
        url = "url",
        fileName = "title",
        filePath = "url",
        formattedSize = "77",
        displayedShortUrl = "url",
        contentType = "jpg",
        status = DownloadState.Status.COMPLETED,
        createdTime = CreatedTime.OLDER,
    )
    private val newFileItem = FileItem(
        id = "1",
        url = "url",
        fileName = "title",
        filePath = "url",
        formattedSize = "77",
        displayedShortUrl = "url",
        contentType = "jpg",
        status = DownloadState.Status.COMPLETED,
        createdTime = CreatedTime.OLDER,
    )

    @Test
    fun exitEditMode() {
        val initialState = oneItemEditState()
        val store = DownloadUIStore(initialState)

        store.dispatch(DownloadUIAction.ExitEditMode)
        assertNotSame(initialState, store.state)
        assertEquals(store.state.mode, DownloadUIState.Mode.Normal)
    }

    @Test
    fun itemAddedForRemoval() {
        val initialState = emptyDefaultState()
        val store = DownloadUIStore(initialState)

        store.dispatch(DownloadUIAction.AddItemForRemoval(newFileItem))
        assertNotSame(initialState, store.state)
        assertEquals(
            store.state.mode,
            DownloadUIState.Mode.Editing(setOf(newFileItem)),
        )
    }

    @Test
    fun allItemsAddedForRemoval() {
        val initialState = DownloadUIState(
            items = listOf(fileItem, newFileItem),
            mode = DownloadUIState.Mode.Normal,
            pendingDeletionIds = emptySet(),
        )
        val store = DownloadUIStore(initialState)

        store.dispatch(DownloadUIAction.AddAllItemsForRemoval)

        val expected = DownloadUIState(
            items = listOf(fileItem, newFileItem),
            mode = DownloadUIState.Mode.Editing(setOf(fileItem, newFileItem)),
            pendingDeletionIds = emptySet(),
        )

        assertEquals(expected, store.state)
    }

    @Test
    fun removeItemForRemoval() {
        val initialState = twoItemEditState()
        val store = DownloadUIStore(initialState)

        store.dispatch(DownloadUIAction.RemoveItemForRemoval(newFileItem))
        assertNotSame(initialState, store.state)
        assertEquals(store.state.mode, DownloadUIState.Mode.Editing(setOf(fileItem)))
    }

    @Test
    fun shareUrlClicked() {
        val initialState = oneItemDefaultState()
        val store = DownloadUIStore(initialState)

        store.dispatch(DownloadUIAction.ShareUrlClicked(fileItem.url))
        assertSame(initialState, store.state)
    }

    @Test
    fun shareFileClicked() {
        val initialState = oneItemDefaultState()
        val store = DownloadUIStore(initialState)

        store.dispatch(DownloadUIAction.ShareFileClicked(fileItem.filePath, fileItem.contentType))
        assertSame(initialState, store.state)
    }

    @Test
    fun `WHEN downloads store is initialised THEN downloads state is updated to be sorted by created time`() {
        val fakeDateTimeProvider = FakeDateTimeProvider(LocalDate.of(2025, 5, 31))
        val zoneId = fakeDateTimeProvider.currentZoneId()

        val downloads = mapOf(
            "1" to DownloadState(
                id = "1",
                url = "https://www.google.com",
                createdTime = LocalDate.of(2025, 3, 1).toEpochMilli(zoneId),
                fileName = "1.pdf",
                status = DownloadState.Status.COMPLETED,
                contentLength = 10000,
                destinationDirectory = "",
                directoryPath = "downloads",
                contentType = "application/pdf",
            ),
            "2" to DownloadState(
                id = "2",
                url = "https://www.google.com",
                createdTime = LocalDate.of(2025, 4, 12).toEpochMilli(zoneId),
                fileName = "2.pdf",
                status = DownloadState.Status.FAILED,
                contentLength = 10000,
                destinationDirectory = "",
                directoryPath = "",
            ),
            "3" to DownloadState(
                id = "3",
                createdTime = LocalDate.of(2025, 5, 31).toEpochMilli(zoneId),
                url = "https://www.google.com",
                fileName = "3.pdf",
                status = DownloadState.Status.COMPLETED,
                contentLength = 10000,
                destinationDirectory = "",
                directoryPath = "downloads",
                contentType = "text/plain",
            ),
        )

        val browserStore = BrowserStore(
            initialState = BrowserState(downloads = downloads),
        )

        val downloadsStore = DownloadUIStore(
            initialState = DownloadUIState.INITIAL,
            middleware = listOf(
                DownloadUIMapperMiddleware(
                    browserStore = browserStore,
                    fileSizeFormatter = fakeFormatter,
                    scope = scope,
                    dateTimeProvider = fakeDateTimeProvider,
                ),
            ),
        )
        downloadsStore.waitUntilIdle()

        val expectedList =
            listOf(
                HeaderItem(CreatedTime.TODAY),
                FileItem(
                    id = "3",
                    url = "https://www.google.com",
                    fileName = "3.pdf",
                    filePath = "downloads/3.pdf",
                    formattedSize = "10000",
                    displayedShortUrl = "google.com",
                    contentType = "text/plain",
                    status = DownloadState.Status.COMPLETED,
                    createdTime = CreatedTime.TODAY,
                ),
                HeaderItem(CreatedTime.OLDER),
                FileItem(
                    id = "1",
                    url = "https://www.google.com",
                    fileName = "1.pdf",
                    filePath = "downloads/1.pdf",
                    formattedSize = "10000",
                    displayedShortUrl = "google.com",
                    contentType = "application/pdf",
                    status = DownloadState.Status.COMPLETED,
                    createdTime = CreatedTime.OLDER,
                ),
            )

        assertEquals(expectedList, downloadsStore.state.itemsToDisplay)
    }

    @Test
    fun `WHEN two download states point to the same existing file THEN only one download item is displayed`() {
        val downloads = mapOf(
            "1" to DownloadState(
                id = "1",
                createdTime = 1,
                url = "https://www.google.com",
                fileName = "1.pdf",
                status = DownloadState.Status.COMPLETED,
                contentLength = 10000,
                destinationDirectory = "",
                directoryPath = "downloads",
                contentType = "application/pdf",
            ),
            "2" to DownloadState(
                id = "2",
                createdTime = 2,
                url = "https://www.google.com",
                fileName = "1.pdf",
                status = DownloadState.Status.COMPLETED,
                destinationDirectory = "",
                contentLength = 10000,
                directoryPath = "downloads",
                contentType = "application/pdf",
            ),
        )
        val browserStore = BrowserStore(initialState = BrowserState(downloads = downloads))

        val downloadsStore = DownloadUIStore(
            initialState = DownloadUIState.INITIAL,
            middleware = listOf(
                DownloadUIMapperMiddleware(
                    browserStore = browserStore,
                    fileSizeFormatter = fakeFormatter,
                    scope = scope,
                ),
            ),
        )
        downloadsStore.waitUntilIdle()

        val expectedList = listOf(
            HeaderItem(CreatedTime.OLDER),
            FileItem(
                id = "1",
                url = "https://www.google.com",
                fileName = "1.pdf",
                filePath = "downloads/1.pdf",
                formattedSize = "10000",
                displayedShortUrl = "google.com",
                contentType = "application/pdf",
                status = DownloadState.Status.COMPLETED,
                createdTime = CreatedTime.OLDER,
            ),
        )

        assertEquals(expectedList, downloadsStore.state.itemsToDisplay)
    }

    private fun LocalDate.toEpochMilli(zoneId: ZoneId): Long {
        return atStartOfDay(zoneId).toInstant().toEpochMilli()
    }

    private fun emptyDefaultState(): DownloadUIState = DownloadUIState(
        items = listOf(),
        mode = DownloadUIState.Mode.Normal,
        pendingDeletionIds = emptySet(),
    )

    private fun oneItemDefaultState(): DownloadUIState = DownloadUIState(
        items = listOf(fileItem),
        mode = DownloadUIState.Mode.Normal,
        pendingDeletionIds = emptySet(),
    )

    private fun oneItemEditState(): DownloadUIState = DownloadUIState(
        items = listOf(),
        mode = DownloadUIState.Mode.Editing(setOf(fileItem)),
        pendingDeletionIds = emptySet(),
    )

    private fun twoItemEditState(): DownloadUIState = DownloadUIState(
        items = listOf(),
        mode = DownloadUIState.Mode.Editing(setOf(fileItem, newFileItem)),
        pendingDeletionIds = emptySet(),
    )
}
