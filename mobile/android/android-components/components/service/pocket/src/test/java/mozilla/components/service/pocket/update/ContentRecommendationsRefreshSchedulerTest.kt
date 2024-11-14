/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.update

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.NetworkType
import androidx.work.PeriodicWorkRequest
import androidx.work.WorkManager
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient
import mozilla.components.service.pocket.PocketStoriesConfig
import mozilla.components.service.pocket.update.ContentRecommendationsRefreshWorker.Companion.REFRESH_WORK_TAG
import mozilla.components.support.base.worker.Frequency
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import java.util.concurrent.TimeUnit

@RunWith(AndroidJUnit4::class)
class ContentRecommendationsRefreshSchedulerTest {

    @Test
    fun `WHEN periodic work is started THEN work is queued`() {
        val client: HttpURLConnectionClient = mock()
        val scheduler = spy(
            ContentRecommendationsRefreshScheduler(
                config = PocketStoriesConfig(
                    client = client,
                    frequency = Frequency(1, TimeUnit.HOURS),
                ),
            ),
        )
        val workManager = mock<WorkManager>()
        val worker = mock<PeriodicWorkRequest>()
        doReturn(workManager).`when`(scheduler).getWorkManager(any())
        doReturn(worker).`when`(scheduler).createPeriodicWorkRequest(any())

        scheduler.startPeriodicWork(testContext)

        verify(workManager).enqueueUniquePeriodicWork(
            REFRESH_WORK_TAG,
            ExistingPeriodicWorkPolicy.KEEP,
            worker,
        )
    }

    @Test
    fun `WHEN periodic work is stopped THEN cancel all work`() {
        val scheduler = spy(ContentRecommendationsRefreshScheduler(mock()))
        val workManager = mock<WorkManager>()
        doReturn(workManager).`when`(scheduler).getWorkManager(any())

        scheduler.stopPeriodicWork(testContext)

        verify(workManager).cancelAllWorkByTag(REFRESH_WORK_TAG)
        verify(workManager, Mockito.never()).cancelAllWork()
    }

    @Test
    fun `WHEN periodic work request is created THEN ensure the work request has the correct constraints configured`() {
        val scheduler = spy(ContentRecommendationsRefreshScheduler(mock()))

        val result = scheduler.createPeriodicWorkRequest(
            Frequency(1, TimeUnit.HOURS),
        )

        verify(scheduler).getWorkerConstraints()
        assertTrue(result.workSpec.intervalDuration == TimeUnit.HOURS.toMillis(1))
        assertFalse(result.workSpec.constraints.requiresBatteryNotLow())
        assertFalse(result.workSpec.constraints.requiresCharging())
        assertFalse(result.workSpec.constraints.hasContentUriTriggers())
        assertFalse(result.workSpec.constraints.requiresStorageNotLow())
        assertFalse(result.workSpec.constraints.requiresDeviceIdle())
        assertTrue(result.workSpec.constraints.requiredNetworkType == NetworkType.CONNECTED)
        assertTrue(result.tags.contains(REFRESH_WORK_TAG))
    }

    @Test
    fun `WHEN worker constraints is created THEN worker constraints should have connected network`() {
        val scheduler = ContentRecommendationsRefreshScheduler(mock())

        val result = scheduler.getWorkerConstraints()

        assertFalse(result.requiresBatteryNotLow())
        assertFalse(result.requiresCharging())
        assertFalse(result.hasContentUriTriggers())
        assertFalse(result.requiresStorageNotLow())
        assertFalse(result.requiresDeviceIdle())
        assertTrue(result.requiredNetworkType == NetworkType.CONNECTED)
    }
}
