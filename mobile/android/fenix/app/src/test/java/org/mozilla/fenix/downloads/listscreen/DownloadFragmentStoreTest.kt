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

@RunWith(AndroidJUnit4::class)
class DownloadFragmentStoreTest {
    @get:Rule
    val coroutineTestRule = MainCoroutineRule()

    private val downloadItem = DownloadItem(
        id = "0",
        url = "url",
        fileName = "title",
        filePath = "url",
        formattedSize = "77",
        contentType = "jpg",
        status = DownloadState.Status.COMPLETED,
    )
    private val newDownloadItem = DownloadItem(
        id = "1",
        url = "url",
        fileName = "title",
        filePath = "url",
        formattedSize = "77",
        contentType = "jpg",
        status = DownloadState.Status.COMPLETED,
    )

    @Test
    fun exitEditMode() {
        val initialState = oneItemEditState()
        val store = DownloadFragmentStore(initialState)

        store.dispatch(DownloadFragmentAction.ExitEditMode)
        assertNotSame(initialState, store.state)
        assertEquals(store.state.mode, DownloadFragmentState.Mode.Normal)
    }

    @Test
    fun itemAddedForRemoval() {
        val initialState = emptyDefaultState()
        val store = DownloadFragmentStore(initialState)

        store.dispatch(DownloadFragmentAction.AddItemForRemoval(newDownloadItem))
        assertNotSame(initialState, store.state)
        assertEquals(
            store.state.mode,
            DownloadFragmentState.Mode.Editing(setOf(newDownloadItem)),
        )
    }

    @Test
    fun allItemsAddedForRemoval() {
        val initialState = DownloadFragmentState(
            items = listOf(downloadItem, newDownloadItem),
            mode = DownloadFragmentState.Mode.Normal,
            pendingDeletionIds = emptySet(),
            isDeletingItems = false,
        )
        val store = DownloadFragmentStore(initialState)

        store.dispatch(DownloadFragmentAction.AddAllItemsForRemoval)

        val expected = DownloadFragmentState(
            items = listOf(downloadItem, newDownloadItem),
            mode = DownloadFragmentState.Mode.Editing(setOf(downloadItem, newDownloadItem)),
            pendingDeletionIds = emptySet(),
            isDeletingItems = false,
        )

        assertEquals(expected, store.state)
    }

    @Test
    fun removeItemForRemoval() {
        val initialState = twoItemEditState()
        val store = DownloadFragmentStore(initialState)

        store.dispatch(DownloadFragmentAction.RemoveItemForRemoval(newDownloadItem))
        assertNotSame(initialState, store.state)
        assertEquals(store.state.mode, DownloadFragmentState.Mode.Editing(setOf(downloadItem)))
    }

    private fun emptyDefaultState(): DownloadFragmentState = DownloadFragmentState(
        items = listOf(),
        mode = DownloadFragmentState.Mode.Normal,
        pendingDeletionIds = emptySet(),
        isDeletingItems = false,
    )

    private fun oneItemEditState(): DownloadFragmentState = DownloadFragmentState(
        items = listOf(),
        mode = DownloadFragmentState.Mode.Editing(setOf(downloadItem)),
        pendingDeletionIds = emptySet(),
        isDeletingItems = false,
    )

    private fun twoItemEditState(): DownloadFragmentState = DownloadFragmentState(
        items = listOf(),
        mode = DownloadFragmentState.Mode.Editing(setOf(downloadItem, newDownloadItem)),
        pendingDeletionIds = emptySet(),
        isDeletingItems = false,
    )
}
