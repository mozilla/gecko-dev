package org.mozilla.fenix.downloads.listscreen.middleware

import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.Downloads
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIAction
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIState
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIStore
import org.mozilla.fenix.downloads.listscreen.store.FileItem
import org.mozilla.fenix.helpers.FenixGleanTestRule
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class DownloadTelemetryMiddlewareTest {
    @get:Rule
    val gleanTestRule = FenixGleanTestRule(testContext)

    @Test
    fun `WHEN the user enters in DownloadScreen THEN record screen viewed telemetry`() {
        assertNull(Downloads.screenViewed.testGetValue())

        createStore()

        assertNotNull(Downloads.screenViewed.testGetValue())
        val snapshot = Downloads.screenViewed.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("screen_viewed", snapshot.single().name)
    }

    @Test
    fun `WHEN the user successfully deletes a file item THEN record deleted telemetry`() {
        val store = createStore()

        assertNull(Downloads.deleted.testGetValue())

        store.dispatch(DownloadUIAction.FileItemDeletedSuccessfully)

        assertNotNull(Downloads.deleted.testGetValue())
        val snapshot = Downloads.deleted.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("deleted", snapshot.single().name)
    }

    @Test
    fun `WHEN the user select a different content type THEN record filtered telemetry`() {
        val store = createStore()

        assertNull(Downloads.filtered.testGetValue())

        store.dispatch(DownloadUIAction.ContentTypeSelected(FileItem.ContentTypeFilter.Other))

        assertNotNull(Downloads.filtered.testGetValue())
        val snapshot = Downloads.filtered.testGetValue()!!
        assertEquals(FileItem.ContentTypeFilter.Other.name, snapshot)
    }

    @Test
    fun `WHEN the user share the downloaded file THEN record share file telemetry`() {
        val store = createStore()

        assertNull(Downloads.shareFile.testGetValue())

        store.dispatch(DownloadUIAction.ShareFileClicked(filePath = "path", contentType = ""))

        assertNotNull(Downloads.shareFile.testGetValue())
        val snapshot = Downloads.shareFile.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("share_file", snapshot.single().name)
    }

    @Test
    fun `WHEN the user share the downloaded file url THEN record share url telemetry`() {
        val store = createStore()

        assertNull(Downloads.shareUrl.testGetValue())

        store.dispatch(DownloadUIAction.ShareUrlClicked("url"))

        assertNotNull(Downloads.shareUrl.testGetValue())
        val snapshot = Downloads.shareUrl.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("share_url", snapshot.single().name)
    }

    @Test
    fun `WHEN the user pauses a downloading file THEN record pause download telemetry`() {
        val store = createStore()

        assertNull(Downloads.pauseDownload.testGetValue())

        store.dispatch(DownloadUIAction.PauseDownload("id"))

        assertNotNull(Downloads.pauseDownload.testGetValue())
        val snapshot = Downloads.pauseDownload.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("pause_download", snapshot.single().name)
    }

    @Test
    fun `WHEN the user resumes downloading a file THEN record resume download telemetry`() {
        val store = createStore()

        assertNull(Downloads.resumeDownload.testGetValue())

        store.dispatch(DownloadUIAction.ResumeDownload("id"))

        assertNotNull(Downloads.resumeDownload.testGetValue())
        val snapshot = Downloads.resumeDownload.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("resume_download", snapshot.single().name)
    }

    @Test
    fun `WHEN the user retries to download a failed download file THEN record retry download telemetry`() {
        val store = createStore()

        assertNull(Downloads.retryDownload.testGetValue())

        store.dispatch(DownloadUIAction.RetryDownload("id"))

        assertNotNull(Downloads.retryDownload.testGetValue())
        val snapshot = Downloads.retryDownload.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("retry_download", snapshot.single().name)
    }

    @Test
    fun `WHEN the user cancels downloading a file THEN record cancel download telemetry`() {
        val store = createStore()

        assertNull(Downloads.cancelDownload.testGetValue())

        store.dispatch(DownloadUIAction.CancelDownload("id"))

        assertNotNull(Downloads.cancelDownload.testGetValue())
        val snapshot = Downloads.cancelDownload.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("cancel_download", snapshot.single().name)
    }

    private fun createStore(
        downloadUIState: DownloadUIState = DownloadUIState.INITIAL,
    ) = DownloadUIStore(
        initialState = downloadUIState,
        middleware = listOf(
            DownloadTelemetryMiddleware(),
        ),
    )
}
