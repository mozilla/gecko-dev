/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import junit.framework.TestCase.assertEquals
import junit.framework.TestCase.assertNull
import mozilla.components.feature.downloads.fake.FakeDateTimeProvider
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
}
