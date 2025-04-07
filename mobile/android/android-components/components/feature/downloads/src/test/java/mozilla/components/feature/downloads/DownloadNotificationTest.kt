/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.app.PendingIntent
import android.os.Build
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationCompat.EXTRA_PROGRESS
import androidx.core.app.NotificationCompat.EXTRA_PROGRESS_INDETERMINATE
import androidx.core.app.NotificationCompat.EXTRA_PROGRESS_MAX
import androidx.core.content.ContextCompat
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.feature.downloads.AbstractFetchDownloadService.DownloadJobState
import mozilla.components.feature.downloads.fake.FakeFileSizeFormatter
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
class DownloadNotificationTest {

    private val fakeFileSizeFormatter: FileSizeFormatter = FakeFileSizeFormatter()

    @Test
    fun getProgress() {
        val downloadJobState = DownloadJobState(
            job = null,
            state = DownloadState(
                url = "mozilla.org/mozilla.txt",
                contentLength = 100L,
                currentBytesCopied = 10,
                status = DownloadState.Status.DOWNLOADING,
            ),
            foregroundServiceId = 1,
            downloadDeleted = false,
            currentBytesCopied = 10,
            status = DownloadState.Status.DOWNLOADING,
        )

        assertEquals(
            "10 / 100",
            downloadJobState.getProgress(
                fileSizeFormatter = fakeFileSizeFormatter,
            ),
        )

        val newDownload = downloadJobState.copy(state = downloadJobState.state.copy(contentLength = null))

        assertEquals(
            "",
            newDownload.getProgress(
                fileSizeFormatter = fakeFileSizeFormatter,
            ),
        )

        val downloadWithNoSize = downloadJobState.copy(state = downloadJobState.state.copy(contentLength = 0))

        assertEquals(
            "",
            downloadWithNoSize.getProgress(
                fileSizeFormatter = fakeFileSizeFormatter,
            ),
        )

        val downloadWithNullSize = downloadJobState.copy(state = downloadJobState.state.copy(contentLength = null))

        assertEquals(
            "",
            downloadWithNullSize.getProgress(
                fileSizeFormatter = fakeFileSizeFormatter,
            ),
        )
    }

    @Test
    fun setCompatGroup() {
        val notificationBuilder = NotificationCompat.Builder(testContext, "")
            .setCompatGroup("myGroup").build()

        assertEquals("myGroup", notificationBuilder.group)
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.M])
    fun `setCompatGroup will not set the group`() {
        val notificationBuilder = NotificationCompat.Builder(testContext, "")
            .setCompatGroup("myGroup").build()

        assertNotEquals("myGroup", notificationBuilder.group)
    }

    @Test
    fun getStatusDescription() {
        val pausedText = testContext.getString(R.string.mozac_feature_downloads_paused_notification_text)
        val completedText = testContext.getString(R.string.mozac_feature_downloads_completed_notification_text2)
        val failedText = testContext.getString(R.string.mozac_feature_downloads_failed_notification_text2)
        var downloadJobState = DownloadJobState(
            job = null,
            state = DownloadState(
                fileName = "mozilla.txt",
                url = "mozilla.org/mozilla.txt",
                contentLength = 100L,
                currentBytesCopied = 10,
                status = DownloadState.Status.DOWNLOADING,
            ),
            foregroundServiceId = 1,
            downloadDeleted = false,
            status = DownloadState.Status.DOWNLOADING,
            currentBytesCopied = 10,
        )

        assertEquals(
            downloadJobState.getProgress(
                fakeFileSizeFormatter,
            ),
            downloadJobState.getStatusDescription(
                context = testContext,
                fileSizeFormatter = fakeFileSizeFormatter,
            ),
        )

        downloadJobState = DownloadJobState(
            job = null,
            state = DownloadState(
                fileName = "mozilla.txt",
                url = "mozilla.org/mozilla.txt",
                contentLength = 100L,
                currentBytesCopied = 10,
                status = DownloadState.Status.PAUSED,
            ),
            foregroundServiceId = 1,
            downloadDeleted = false,
            status = DownloadState.Status.PAUSED,
        )

        assertEquals(
            pausedText,
            downloadJobState.getStatusDescription(
                context = testContext,
                fileSizeFormatter = fakeFileSizeFormatter,
            ),
        )

        downloadJobState = DownloadJobState(
            job = null,
            state = DownloadState(
                fileName = "mozilla.txt",
                url = "mozilla.org/mozilla.txt",
                contentLength = 100L,
                currentBytesCopied = 10,
                status = DownloadState.Status.COMPLETED,
            ),
            foregroundServiceId = 1,
            downloadDeleted = false,
            status = DownloadState.Status.COMPLETED,
        )

        assertEquals(
            completedText,
            downloadJobState.getStatusDescription(
                context = testContext,
                fileSizeFormatter = fakeFileSizeFormatter,
            ),
        )

        downloadJobState = DownloadJobState(
            job = null,
            state = DownloadState(
                fileName = "mozilla.txt",
                url = "mozilla.org/mozilla.txt",
                contentLength = 100L,
                currentBytesCopied = 10,
                status = DownloadState.Status.FAILED,
            ),
            foregroundServiceId = 1,
            downloadDeleted = false,
            status = DownloadState.Status.FAILED,
        )

        assertEquals(
            failedText,
            downloadJobState.getStatusDescription(
                context = testContext,
                fileSizeFormatter = fakeFileSizeFormatter,
            ),
        )

        downloadJobState = DownloadJobState(
            job = null,
            state = DownloadState(
                fileName = "mozilla.txt",
                url = "mozilla.org/mozilla.txt",
                contentLength = 100L,
                currentBytesCopied = 10,
                status = DownloadState.Status.CANCELLED,
            ),
            foregroundServiceId = 1,
            downloadDeleted = false,
            status = DownloadState.Status.CANCELLED,
        )

        assertEquals(
            "",
            downloadJobState.getStatusDescription(
                context = testContext,
                fileSizeFormatter = fakeFileSizeFormatter,
            ),
        )
    }

    @Test
    fun getDownloadSummary() {
        val download1 = DownloadJobState(
            job = null,
            state = DownloadState(
                fileName = "mozilla.txt",
                url = "mozilla.org/mozilla.txt",
                contentLength = 100L,
                currentBytesCopied = 10,
                status = DownloadState.Status.DOWNLOADING,
            ),
            foregroundServiceId = 1,
            downloadDeleted = false,
            currentBytesCopied = 10,
            status = DownloadState.Status.DOWNLOADING,
        )
        val download2 = DownloadJobState(
            job = null,
            state = DownloadState(
                fileName = "mozilla2.txt",
                url = "mozilla.org/mozilla.txt",
                contentLength = 100L,
                currentBytesCopied = 20,
                status = DownloadState.Status.DOWNLOADING,
            ),
            foregroundServiceId = 1,
            downloadDeleted = false,
            currentBytesCopied = 20,
            status = DownloadState.Status.DOWNLOADING,
        )

        val summary = DownloadNotification.getSummaryList(
            context = testContext,
            fileSizeFormatter = fakeFileSizeFormatter,
            notifications = listOf(download1, download2),
        )
        assertEquals(listOf("mozilla.txt 10 / 100", "mozilla2.txt 20 / 100"), summary)
    }

    @Test
    fun `createOngoingDownloadNotification progress does not overflow`() {
        val size = 3 * 1024L * 1024L * 1024L
        val copiedSize = size / 2
        val downloadJobState = DownloadJobState(
            job = null,
            state = DownloadState(
                fileName = "mozilla.txt",
                url = "mozilla.org/mozilla.txt",
                contentLength = size,
                currentBytesCopied = copiedSize,
                status = DownloadState.Status.DOWNLOADING,
            ),
            foregroundServiceId = 1,
            downloadDeleted = false,
            currentBytesCopied = copiedSize,
            status = DownloadState.Status.DOWNLOADING,
        )

        val style = AbstractFetchDownloadService.Style()

        val notification = DownloadNotification.createOngoingDownloadNotification(
            context = testContext,
            downloadJobState = downloadJobState,
            fileSizeFormatter = fakeFileSizeFormatter,
            notificationAccentColor = style.notificationAccentColor,
        )

        assertEquals(
            50L,
            100L * notification.extras.getInt(EXTRA_PROGRESS) / notification.extras.getInt(EXTRA_PROGRESS_MAX),
        )

        assertEquals(false, notification.extras.getBoolean(EXTRA_PROGRESS_INDETERMINATE))

        val notificationNewDownload = DownloadNotification.createOngoingDownloadNotification(
            context = testContext,
            downloadJobState = downloadJobState.copy(state = downloadJobState.state.copy(contentLength = null)),
            fileSizeFormatter = fakeFileSizeFormatter,
            notificationAccentColor = style.notificationAccentColor,
        )

        assertEquals(true, notificationNewDownload.extras.getBoolean(EXTRA_PROGRESS_INDETERMINATE))

        val notificationDownloadWithNoSize = DownloadNotification.createOngoingDownloadNotification(
            context = testContext,
            downloadJobState = downloadJobState.copy(state = downloadJobState.state.copy(contentLength = 0)),
            fileSizeFormatter = fakeFileSizeFormatter,
            notificationAccentColor = style.notificationAccentColor,
        )

        assertEquals(true, notificationDownloadWithNoSize.extras.getBoolean(EXTRA_PROGRESS_INDETERMINATE))
    }

    @Test
    fun getOngoingNotificationAccentColor() {
        val download = DownloadJobState(
            job = null,
            state = DownloadState(
                fileName = "mozilla.txt",
                url = "mozilla.org/mozilla.txt",
                contentLength = 100L,
                currentBytesCopied = 10,
                status = DownloadState.Status.DOWNLOADING,
            ),
            foregroundServiceId = 1,
            downloadDeleted = false,
            currentBytesCopied = 10,
            status = DownloadState.Status.DOWNLOADING,
        )

        val style = AbstractFetchDownloadService.Style()

        val notification = DownloadNotification.createOngoingDownloadNotification(
            context = testContext,
            downloadJobState = download,
            fileSizeFormatter = fakeFileSizeFormatter,
            notificationAccentColor = style.notificationAccentColor,
        )

        val accentColor = ContextCompat.getColor(testContext, style.notificationAccentColor)

        assertEquals(accentColor, notification.color)
    }

    @Test
    fun getPausedNotificationAccentColor() {
        val download = DownloadJobState(
            job = null,
            state = DownloadState(
                fileName = "mozilla.txt",
                url = "mozilla.org/mozilla.txt",
                contentLength = 100L,
                currentBytesCopied = 10,
                status = DownloadState.Status.PAUSED,
            ),
            foregroundServiceId = 1,
            downloadDeleted = false,
            currentBytesCopied = 10,
            status = DownloadState.Status.PAUSED,
        )

        val style = AbstractFetchDownloadService.Style()

        val notification = DownloadNotification.createPausedDownloadNotification(
            testContext,
            download,
            notificationAccentColor = style.notificationAccentColor,
        )

        val accentColor = ContextCompat.getColor(testContext, style.notificationAccentColor)

        assertEquals(accentColor, notification.color)
    }

    @Test
    fun getCompletedNotificationAccentColor() {
        val download = DownloadJobState(
            job = null,
            state = DownloadState(
                fileName = "mozilla.txt",
                url = "mozilla.org/mozilla.txt",
                contentLength = 100L,
                currentBytesCopied = 10,
                status = DownloadState.Status.COMPLETED,
            ),
            foregroundServiceId = 1,
            downloadDeleted = false,
            currentBytesCopied = 10,
            status = DownloadState.Status.COMPLETED,
        )

        val style = AbstractFetchDownloadService.Style()

        val notification = DownloadNotification.createDownloadCompletedNotification(
            testContext,
            download,
            notificationAccentColor = style.notificationAccentColor,
            mock(PendingIntent::class.java),
        )

        val accentColor = ContextCompat.getColor(testContext, style.notificationAccentColor)

        assertEquals(accentColor, notification.color)
    }

    @Test
    fun getFailedNotificationAccentColor() {
        val download = DownloadJobState(
            job = null,
            state = DownloadState(
                fileName = "mozilla.txt",
                url = "mozilla.org/mozilla.txt",
                contentLength = 100L,
                currentBytesCopied = 10,
                status = DownloadState.Status.FAILED,
            ),
            foregroundServiceId = 1,
            downloadDeleted = false,
            currentBytesCopied = 10,
            status = DownloadState.Status.FAILED,
        )

        val style = AbstractFetchDownloadService.Style()

        val notification = DownloadNotification.createDownloadFailedNotification(
            testContext,
            download,
            notificationAccentColor = style.notificationAccentColor,
        )

        val accentColor = ContextCompat.getColor(testContext, style.notificationAccentColor)

        assertEquals(accentColor, notification.color)
    }

    @Test
    fun getGroupNotificationAccentColor() {
        val download1 = DownloadJobState(
            job = null,
            state = DownloadState(
                fileName = "mozilla.txt",
                url = "mozilla.org/mozilla.txt",
                contentLength = 100L,
                currentBytesCopied = 10,
                status = DownloadState.Status.DOWNLOADING,
            ),
            foregroundServiceId = 1,
            downloadDeleted = false,
            currentBytesCopied = 10,
            status = DownloadState.Status.DOWNLOADING,
        )

        val download2 = DownloadJobState(
            job = null,
            state = DownloadState(
                fileName = "mozilla.txt",
                url = "mozilla.org/mozilla.txt",
                contentLength = 100L,
                currentBytesCopied = 10,
                status = DownloadState.Status.DOWNLOADING,
            ),
            foregroundServiceId = 1,
            downloadDeleted = false,
            currentBytesCopied = 10,
            status = DownloadState.Status.DOWNLOADING,
        )

        val style = AbstractFetchDownloadService.Style()

        val notification = DownloadNotification.createDownloadGroupNotification(
            context = testContext,
            fileSizeFormatter = fakeFileSizeFormatter,
            notifications = listOf(download1, download2),
            notificationAccentColor = style.notificationAccentColor,
        )

        val accentColor = ContextCompat.getColor(testContext, style.notificationAccentColor)

        assertEquals(accentColor, notification.color)
    }
}
