/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

private const val NUM_MS_IN_SEC = 1000f

/**
 * Utility class for estimating the download time remaining.
 *
 * @param totalBytes The total file size in bytes.
 * @param dateTimeProvider The [DateTimeProvider] used to get the current time.
 */
class DownloadEstimator(
    private val totalBytes: Long,
    private val dateTimeProvider: DateTimeProvider,
) {

    /**
     * The start time of the download in milliseconds.
     */
    private val startTime = dateTimeProvider.currentTimeMillis()

    /**
     * Returns estimated time remaining for download time complete in seconds.
     *
     * @param bytesDownloaded The amount of bytes downloaded so far.
     * @param currentTime The current time in milliseconds.
     */
    fun estimatedRemainingTime(
        bytesDownloaded: Long,
    ): Long? {
        if (bytesDownloaded <= 0L || totalBytes <= 0L || totalBytes < bytesDownloaded) return null
        val timeDeltaInSecs: Float = (dateTimeProvider.currentTimeMillis() - startTime) / NUM_MS_IN_SEC

        // An estimate cannot be provided if no time has passed since the download started
        if (timeDeltaInSecs > 0) {
            val bytesPerSec = bytesDownloaded / timeDeltaInSecs

            if (bytesPerSec > 0) {
                val secsRemaining = (totalBytes - bytesDownloaded) / bytesPerSec
                return secsRemaining.toLong()
            }
        }
        return null
    }
}
