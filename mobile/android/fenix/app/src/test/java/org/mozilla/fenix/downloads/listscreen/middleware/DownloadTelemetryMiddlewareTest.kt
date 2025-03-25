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

    private fun createStore(
        downloadUIState: DownloadUIState = DownloadUIState.INITIAL,
    ) = DownloadUIStore(
        initialState = downloadUIState,
        middleware = listOf(
            DownloadTelemetryMiddleware(),
        ),
    )
}
