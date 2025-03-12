/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotSame
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.downloads.listscreen.store.CreatedTime
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIAction
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIState
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIStore
import org.mozilla.fenix.downloads.listscreen.store.FileItem

@RunWith(AndroidJUnit4::class)
class DownloadUIStoreTest {
    @get:Rule
    val coroutineTestRule = MainCoroutineRule()

    private val fileItem = FileItem(
        id = "0",
        url = "url",
        fileName = "title",
        filePath = "url",
        formattedSize = "77",
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
            isDeletingItems = false,
        )
        val store = DownloadUIStore(initialState)

        store.dispatch(DownloadUIAction.AddAllItemsForRemoval)

        val expected = DownloadUIState(
            items = listOf(fileItem, newFileItem),
            mode = DownloadUIState.Mode.Editing(setOf(fileItem, newFileItem)),
            pendingDeletionIds = emptySet(),
            isDeletingItems = false,
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

    private fun emptyDefaultState(): DownloadUIState = DownloadUIState(
        items = listOf(),
        mode = DownloadUIState.Mode.Normal,
        pendingDeletionIds = emptySet(),
        isDeletingItems = false,
    )

    private fun oneItemEditState(): DownloadUIState = DownloadUIState(
        items = listOf(),
        mode = DownloadUIState.Mode.Editing(setOf(fileItem)),
        pendingDeletionIds = emptySet(),
        isDeletingItems = false,
    )

    private fun twoItemEditState(): DownloadUIState = DownloadUIState(
        items = listOf(),
        mode = DownloadUIState.Mode.Editing(setOf(fileItem, newFileItem)),
        pendingDeletionIds = emptySet(),
        isDeletingItems = false,
    )
}
