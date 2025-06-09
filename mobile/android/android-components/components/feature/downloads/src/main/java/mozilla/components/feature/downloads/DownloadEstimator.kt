/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

private const val NUM_MS_IN_SEC = 1000

/**
 * Utility class for estimating the download time remaining.
 *
 * @param dateTimeProvider The [DateTimeProvider] used to get the current time.
 */
class DownloadEstimator(
    private val dateTimeProvider: DateTimeProvider,
) {

    /**
     * Returns estimated time remaining for download completion in seconds.
     *
     * @param startTime The start time of the download in milliseconds.
     * @param bytesDownloaded The amount of bytes downloaded so far.
     * @param totalBytes The total file size in bytes.
     */
    fun estimatedRemainingTime(
        startTime: Long,
        bytesDownloaded: Long,
        totalBytes: Long,
    ): Long? {
        if (bytesDownloaded <= 0L || totalBytes <= 0L) return null
        val timeDeltaInSecs = (dateTimeProvider.currentTimeMillis() - startTime) / NUM_MS_IN_SEC
        if (timeDeltaInSecs > 0 && totalBytes >= bytesDownloaded) {
            val bytesPerSec = bytesDownloaded / timeDeltaInSecs

            // We already checked that bytesDownloaded > 0 so bytesPerSec > 0
            val secsRemaining = (totalBytes - bytesDownloaded) / bytesPerSec

            return secsRemaining
        } else {
            // An estimate cannot be provided if no time has passed since the download started
            return null
        }
    }
}
