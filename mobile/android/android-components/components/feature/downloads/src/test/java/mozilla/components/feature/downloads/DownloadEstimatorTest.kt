/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import mozilla.components.feature.downloads.fake.FakeDateTimeProvider
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test

class DownloadEstimatorTest {

    @Test
    fun `GIVEN a 100 kB download has completed 10 kB in 10 seconds so far WHEN calculating the time remaining THEN the estimated time remaining is 90 seconds`() {
        val downloadEstimator = DownloadEstimator(
            dateTimeProvider = FakeDateTimeProvider(currentTime = 10000),
        )

        val actual = downloadEstimator.estimatedRemainingTime(
            startTime = 0,
            bytesDownloaded = 10000,
            totalBytes = 100000,
        )
        assertEquals(90L, actual)
    }

    @Test
    fun `GIVEN a 10 MB download has completed 100 kB in 10 seconds so far WHEN calculating the time remaining THEN the estimated time remaining is 990 seconds`() {
        val downloadEstimator = DownloadEstimator(
            dateTimeProvider = FakeDateTimeProvider(currentTime = 10000),
        )

        val actual = downloadEstimator.estimatedRemainingTime(
            startTime = 0,
            bytesDownloaded = 100000,
            totalBytes = 10000000,
        )
        assertEquals(990L, actual)
    }

    @Test
    fun `GIVEN a 1 GB download has completed 500 MB in 60 seconds so far WHEN calculating the time remaining THEN the estimated time remaining is 60 seconds`() {
        val downloadEstimator = DownloadEstimator(
            dateTimeProvider = FakeDateTimeProvider(currentTime = 60000),
        )

        val actual = downloadEstimator.estimatedRemainingTime(
            startTime = 0,
            bytesDownloaded = 500000000,
            totalBytes = 1000000000,
        )
        assertEquals(60L, actual)
    }

    @Test
    fun `GIVEN no bytes have been downloaded yet WHEN calculating the time remaining THEN the estimated time remaining cannot be calculated`() {
        val downloadEstimator = DownloadEstimator(
            dateTimeProvider = FakeDateTimeProvider(currentTime = 10000),
        )

        val actual = downloadEstimator.estimatedRemainingTime(
            startTime = 0,
            bytesDownloaded = 0,
            totalBytes = 100000,
        )
        assertNull(actual)
    }

    @Test
    fun `GIVEN total bytes is 0 WHEN calculating the time remaining THEN the estimated time remaining cannot be calculated`() {
        val downloadEstimator = DownloadEstimator(
            dateTimeProvider = FakeDateTimeProvider(currentTime = 10000),
        )

        val actual = downloadEstimator.estimatedRemainingTime(
            startTime = 0,
            bytesDownloaded = 0,
            totalBytes = 0,
        )
        assertNull(actual)
    }

    @Test
    fun `GIVEN total bytes is less than the bytes downloaded WHEN calculating the time remaining THEN the estimated time remaining cannot be calculated`() {
        val downloadEstimator = DownloadEstimator(
            dateTimeProvider = FakeDateTimeProvider(currentTime = 10000),
        )

        val actual = downloadEstimator.estimatedRemainingTime(
            startTime = 0,
            bytesDownloaded = 100,
            totalBytes = 10,
        )
        assertNull(actual)
    }

    @Test
    fun `GIVEN the current time is equal to the start time WHEN calculating the time remaining THEN the estimated time remaining cannot be calculated`() {
        val downloadEstimator = DownloadEstimator(
            dateTimeProvider = FakeDateTimeProvider(currentTime = 0),
        )

        val actual = downloadEstimator.estimatedRemainingTime(
            startTime = 0,
            bytesDownloaded = 100,
            totalBytes = 1000,
        )
        assertNull(actual)
    }

    @Test
    fun `GIVEN the download speed is very slow, meaning the elapsed download time is really large and the bytes downloaded is very small, WHEN calculating the time remaining THEN the estimated time remaining can be calculated`() {
        val downloadEstimator = DownloadEstimator(
            dateTimeProvider = FakeDateTimeProvider(currentTime = Long.MAX_VALUE),
        )

        val actual = downloadEstimator.estimatedRemainingTime(
            startTime = 0,
            bytesDownloaded = 1,
            totalBytes = 2,
        )

        assertNotNull(actual)
    }

    @Test
    fun `GIVEN the download speed is very fast, meaning the elapsed download time is really small and the bytes downloaded is very large, WHEN calculating the time remaining THEN the estimated time remaining can be calculated`() {
        val downloadEstimator = DownloadEstimator(
            dateTimeProvider = FakeDateTimeProvider(currentTime = 1),
        )

        val actual = downloadEstimator.estimatedRemainingTime(
            startTime = 0,
            bytesDownloaded = Long.MAX_VALUE - 1,
            totalBytes = Long.MAX_VALUE,
        )

        assertNotNull(actual)
    }

    @Test
    fun `GIVEN half of the file has been downloaded in 2 seconds WHEN calculating the time remaining THEN the estimated time remaining is 2 seconds`() {
        val downloadEstimator = DownloadEstimator(
            dateTimeProvider = FakeDateTimeProvider(currentTime = 2000),
        )

        val actual = downloadEstimator.estimatedRemainingTime(
            startTime = 0,
            bytesDownloaded = 1,
            totalBytes = 2,
        )
        assertEquals(2L, actual)
    }
}
