/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.update

import androidx.concurrent.futures.await
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.work.ListenableWorker
import androidx.work.testing.TestListenableWorkerBuilder
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.service.pocket.GlobalDependencyProvider
import mozilla.components.service.pocket.mars.SponsoredContentsUseCases
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn

@ExperimentalCoroutinesApi // for runTestOnMain
@RunWith(AndroidJUnit4::class)
class DeleteUserWorkerTest {

    @get:Rule
    val mainCoroutineRule = MainCoroutineRule()

    @Test
    fun `WHEN user profile deletion is successful THEN return success`() = runTestOnMain {
        val useCases: SponsoredContentsUseCases = mock()
        val deleteUser: SponsoredContentsUseCases.DeleteUser = mock()

        doReturn(true).`when`(deleteUser).invoke()
        doReturn(deleteUser).`when`(useCases).deleteUser

        GlobalDependencyProvider.SponsoredContents.initialize(useCases)
        val worker = TestListenableWorkerBuilder<DeleteUserWorker>(testContext).build()

        val result = worker.startWork().await()

        assertEquals(ListenableWorker.Result.success(), result)
    }

    @Test
    fun `WHEN user profile deletion fails THEN worker should retry`() = runTestOnMain {
        val useCases: SponsoredContentsUseCases = mock()
        val deleteUser: SponsoredContentsUseCases.DeleteUser = mock()

        doReturn(false).`when`(deleteUser).invoke()
        doReturn(deleteUser).`when`(useCases).deleteUser

        GlobalDependencyProvider.SponsoredContents.initialize(useCases)
        val worker = TestListenableWorkerBuilder<DeleteUserWorker>(testContext).build()

        val result = worker.startWork().await()

        assertEquals(ListenableWorker.Result.retry(), result)
    }
}
