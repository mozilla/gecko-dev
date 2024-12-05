/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.fenix.splashscreen

import android.content.Context
import android.view.View
import androidx.lifecycle.LifecycleOwner
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.test.advanceUntilIdle
import kotlinx.coroutines.test.runTest
import mozilla.components.service.nimbus.NimbusApi
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.experiments.nimbus.NimbusInterface
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class SplashScreenOperationTest {

    @Test
    fun `GIVEN the nimbus experiment has not fetched the data WHEN fetch operation is called THEN we observe and record nimbus fetching the data`() = runTest {
        val testNimbus = TestNimbusApi(this)
        val operation = FetchExperimentsOperation(
            buildStorage(isDataFetchedAlready = false),
            testNimbus,
        )

        assertNull(operation.fetchNimbusObserver)
        assertTrue(testNimbus.observers.isEmpty())

        launch { operation.run() }
        delay(100)

        assertNotNull(operation.fetchNimbusObserver)
        assertTrue(testNimbus.observers.contains(operation.fetchNimbusObserver))

        launch { testNimbus.fakeExperimentsFetch(0) }
        advanceUntilIdle()

        assertTrue(operation.dataFetched)
    }

    @Test
    fun `GIVEN nimbus data is already fetched WHEN fetch operation is called THEN we do not observe nimbus fetch`() = runTest {
        val operation = FetchExperimentsOperation(
            buildStorage(isDataFetchedAlready = true),
            TestNimbusApi(this),
        )

        operation.run()

        assertNull(operation.fetchNimbusObserver)
        assertTrue(operation.dataFetched)
    }

    @Test
    fun `WHEN fetch operation is finished THEN nimbus callback is unregistered`() = runTest {
        val testNimbus = TestNimbusApi(this)
        val operation = FetchExperimentsOperation(
            buildStorage(),
            testNimbus,
        )

        launch { operation.run() }
        delay(100)

        assertTrue(testNimbus.observers.contains(operation.fetchNimbusObserver))

        launch { testNimbus.fakeExperimentsFetch(100) }
        advanceUntilIdle()

        assertFalse(testNimbus.observers.contains(operation.fetchNimbusObserver))
    }

    @Test
    fun `GIVEN nimbus data not fetched WHEN apply operation is called THEN we observe and record nimbus fetching the data and nimbus applying the data`() = runTest {
        val testNimbus = TestNimbusApi(scope = this, applyDelay = 1000L)
        val operation = ApplyExperimentsOperation(
            buildStorage(isDataFetchedAlready = false),
            testNimbus,
        )

        assertNull(operation.fetchNimbusObserver)
        assertNull(operation.applyNimbusObserver)
        assertTrue(testNimbus.observers.isEmpty())

        launch { operation.run() }
        delay(100)
        assertNotNull(operation.fetchNimbusObserver)
        assertTrue(testNimbus.observers.contains(operation.fetchNimbusObserver))

        launch { testNimbus.fakeExperimentsFetch(0) }
        delay(100)
        assertNotNull(operation.applyNimbusObserver)
        assertTrue(testNimbus.observers.contains(operation.applyNimbusObserver))

        advanceUntilIdle()
        assertTrue(operation.dataFetched)
        assertTrue(operation.isDataApplied)
    }

    @Test
    fun `GIVEN nimbus data already fetched WHEN apply operation is called THEN we do not observe nimbus fetch`() = runTest {
        val operation = ApplyExperimentsOperation(
            buildStorage(isDataFetchedAlready = true),
            TestNimbusApi(this),
        )

        operation.run()

        assertNull(operation.fetchNimbusObserver)
        assertTrue(operation.dataFetched)
    }

    @Test
    fun `WHEN apply operation is finished THEN nimbus callback is unregistered`() = runTest {
        val testNimbus = TestNimbusApi(this)
        val operation = ApplyExperimentsOperation(
            buildStorage(),
            testNimbus,
        )

        launch { operation.run() }
        delay(100)

        assertTrue(testNimbus.observers.contains(operation.fetchNimbusObserver))

        launch { testNimbus.fakeExperimentsFetch(100) }
        delay(200)

        assertTrue(testNimbus.observers.contains(operation.applyNimbusObserver))

        advanceUntilIdle()

        assertFalse(testNimbus.observers.contains(operation.fetchNimbusObserver))
        assertFalse(testNimbus.observers.contains(operation.applyNimbusObserver))
    }

    class TestNimbusApi(
        private val scope: CoroutineScope,
        private val applyDelay: Long = 500L,
    ) : NimbusApi {
        val observers = mutableListOf<NimbusInterface.Observer>()

        override fun register(observer: NimbusInterface.Observer) {
            observers.add(observer)
        }

        override fun register(observer: NimbusInterface.Observer, owner: LifecycleOwner, autoPause: Boolean) {
            // NOOP
        }

        override fun register(observer: NimbusInterface.Observer, view: View) {
            // NOOP
        }

        override fun unregister(observer: NimbusInterface.Observer) {
            observers.remove(observer)
        }

        override fun unregisterObservers() {
            // NOOP
        }

        override fun notifyObservers(block: NimbusInterface.Observer.() -> Unit) {
            // NOOP
        }

        override fun notifyAtLeastOneObserver(block: NimbusInterface.Observer.() -> Unit) {
            // NOOP
        }

        override fun pauseObserver(observer: NimbusInterface.Observer) {
            // NOOP
        }

        override fun resumeObserver(observer: NimbusInterface.Observer) {
            // NOOP
        }

        override fun <R> wrapConsumers(block: NimbusInterface.Observer.(R) -> Boolean): List<(R) -> Boolean> {
            return listOf()
        }

        override fun isObserved(): Boolean {
            return false
        }

        suspend fun fakeExperimentsFetch(fetchDelay: Long) {
            delay(fetchDelay)
            observers.forEach { it.onExperimentsFetched() }
        }

        override fun applyPendingExperiments(): Job {
            return scope.launch {
                delay(applyDelay)
                observers.forEach { it.onUpdatesApplied(listOf()) }
            }
        }

        override val context: Context
            get() = testContext

        override var globalUserParticipation: Boolean = true
    }

    private fun buildStorage(
        isDataFetchedAlready: Boolean = false,
    ) = object : ExperimentsOperationStorage {
        override val nimbusExperimentsFetched: Boolean
            get() = isDataFetchedAlready
    }
}
