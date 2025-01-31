/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.update

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.work.Constraints
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.ExistingWorkPolicy
import androidx.work.NetworkType
import androidx.work.OneTimeWorkRequest
import androidx.work.OneTimeWorkRequestBuilder
import androidx.work.PeriodicWorkRequest
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.WorkManager
import mozilla.components.service.pocket.PocketStoriesConfig
import mozilla.components.service.pocket.logger
import mozilla.components.service.pocket.update.DeleteUserWorker.Companion.DELETE_USER_WORK_TAG
import mozilla.components.service.pocket.update.SponsoredContentsRefreshWorker.Companion.REFRESH_WORK_TAG
import mozilla.components.support.base.worker.Frequency

/**
 * Provides functionality to schedule sponsored contents refresh.
 *
 * @property config Configuration for how sponsored contents should be refreshed.
 */
class SponsoredContentsRefreshScheduler(
    private val config: PocketStoriesConfig,
) {
    internal fun startPeriodicRefreshes(context: Context) {
        getWorkManager(context).enqueueUniquePeriodicWork(
            REFRESH_WORK_TAG,
            ExistingPeriodicWorkPolicy.KEEP,
            createPeriodicRefreshWorkRequest(
                frequency = config.sponsoredStoriesRefreshFrequency,
            ),
        )

        logger.info("Started periodic refreshes of sponsored contents")
    }

    internal fun stopPeriodicRefreshes(context: Context) {
        getWorkManager(context).cancelAllWorkByTag(REFRESH_WORK_TAG)

        logger.info("Stopped periodic refreshes of sponsored contents")
    }

    @VisibleForTesting
    internal fun createPeriodicRefreshWorkRequest(
        frequency: Frequency,
    ): PeriodicWorkRequest {
        val constraints = getWorkerConstraints()

        return PeriodicWorkRequestBuilder<SponsoredContentsRefreshWorker>(
            frequency.repeatInterval,
            frequency.repeatIntervalTimeUnit,
        ).apply {
            setConstraints(constraints)
            addTag(REFRESH_WORK_TAG)
        }.build()
    }

    internal fun scheduleUserDeletion(context: Context) {
        getWorkManager(context).enqueueUniqueWork(
            DELETE_USER_WORK_TAG,
            ExistingWorkPolicy.KEEP,
            createOneTimeDeleteUserWorkerRequest(),
        )

        logger.info("Scheduling sponsored content user deletion")
    }

    internal fun stopUserDeletion(context: Context) {
        getWorkManager(context).cancelAllWorkByTag(DELETE_USER_WORK_TAG)
    }

    @VisibleForTesting
    internal fun createOneTimeDeleteUserWorkerRequest(): OneTimeWorkRequest {
        val constraints = getWorkerConstraints()

        return OneTimeWorkRequestBuilder<DeleteUserWorker>()
            .apply {
                setConstraints(constraints)
                addTag(DELETE_USER_WORK_TAG)
            }
            .build()
    }

    @VisibleForTesting
    internal fun getWorkerConstraints() = Constraints.Builder()
        .setRequiredNetworkType(NetworkType.CONNECTED)
        .build()

    @VisibleForTesting
    internal fun getWorkManager(context: Context) = WorkManager.getInstance(context)
}
