/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.update

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.work.Constraints
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.NetworkType
import androidx.work.PeriodicWorkRequest
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.WorkManager
import mozilla.components.service.pocket.PocketStoriesConfig
import mozilla.components.service.pocket.logger
import mozilla.components.service.pocket.update.ContentRecommendationsRefreshWorker.Companion.REFRESH_WORK_TAG
import mozilla.components.support.base.worker.Frequency

/**
 * Provides functionality to schedule content recommendations refresh.
 *
 * @property config Configuration for how content recommendations should be refreshed.
 */
class ContentRecommendationsRefreshScheduler(
    private val config: PocketStoriesConfig,
) {
    internal fun startPeriodicWork(context: Context) {
        getWorkManager(context).enqueueUniquePeriodicWork(
            REFRESH_WORK_TAG,
            ExistingPeriodicWorkPolicy.KEEP,
            createPeriodicWorkRequest(
                frequency = config.contentRecommendationsRefreshFrequency,
            ),
        )

        logger.info("Started periodic work to refresh content recommendations")
    }

    internal fun stopPeriodicWork(context: Context) {
        getWorkManager(context).cancelAllWorkByTag(REFRESH_WORK_TAG)

        logger.info("Stopped periodic work to refresh content recommendations")
    }

    @VisibleForTesting
    internal fun createPeriodicWorkRequest(
        frequency: Frequency,
    ): PeriodicWorkRequest {
        val constraints = getWorkerConstraints()

        return PeriodicWorkRequestBuilder<ContentRecommendationsRefreshWorker>(
            frequency.repeatInterval,
            frequency.repeatIntervalTimeUnit,
        ).apply {
            setConstraints(constraints)
            addTag(REFRESH_WORK_TAG)
        }.build()
    }

    @VisibleForTesting
    internal fun getWorkerConstraints() = Constraints.Builder()
        .setRequiredNetworkType(NetworkType.CONNECTED)
        .build()

    @VisibleForTesting
    internal fun getWorkManager(context: Context) = WorkManager.getInstance(context)
}
