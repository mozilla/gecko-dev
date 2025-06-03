/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen.middleware

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.feature.downloads.AbstractFetchDownloadService
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIAction
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIState
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIStore

@RunWith(AndroidJUnit4::class)
class DownloadsServiceCommunicationMiddlewareTest {

    private lateinit var store: DownloadUIStore
    private lateinit var broadcastSender: FakeBroadcastSender

    @Before
    fun setup() {
        broadcastSender = FakeBroadcastSender()
        val middleware = DownloadsServiceCommunicationMiddleware(broadcastSender)
        store = DownloadUIStore(
            initialState = DownloadUIState.INITIAL,
            middleware = listOf(
                middleware,
            ),
        )
    }

    private class FakeBroadcastSender : BroadcastSender {
        var downloadId: String? = null
        var action: String? = null
        override fun sendBroadcast(downloadId: String, action: String) {
            this.downloadId = downloadId
            this.action = action
        }
    }

    @Test
    fun `WHEN the user pauses a download THEN the corresponding broadcastIntent will be sent`() {
        store.dispatch(action = DownloadUIAction.PauseDownload("id"))
        assertEquals("id", broadcastSender.downloadId)
        assertEquals(AbstractFetchDownloadService.ACTION_PAUSE, broadcastSender.action)
    }

    @Test
    fun `WHEN the user resumes a download THEN the corresponding broadcastIntent will be sent`() {
        store.dispatch(action = DownloadUIAction.ResumeDownload("id"))
        assertEquals("id", broadcastSender.downloadId)
        assertEquals(AbstractFetchDownloadService.ACTION_RESUME, broadcastSender.action)
    }

    @Test
    fun `WHEN the user cancels a download THEN the corresponding broadcastIntent will be sent`() {
        store.dispatch(action = DownloadUIAction.CancelDownload("id"))
        assertEquals("id", broadcastSender.downloadId)
        assertEquals(AbstractFetchDownloadService.ACTION_CANCEL, broadcastSender.action)
    }

    @Test
    fun `WHEN the user tries to trigger the download again THEN the corresponding broadcastIntent will be sent`() {
        store.dispatch(action = DownloadUIAction.RetryDownload("id"))
        assertEquals("id", broadcastSender.downloadId)
        assertEquals(AbstractFetchDownloadService.ACTION_TRY_AGAIN, broadcastSender.action)
    }
}
