/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.messaging

import android.view.View
import androidx.lifecycle.LifecycleOwner
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.test.runTest
import mozilla.components.service.nimbus.NimbusApi
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.experiments.nimbus.NimbusInterface
import org.mozilla.fenix.messaging.MessageNotificationWorker.Companion.tryFetchAndApplyNimbusExperiments
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class MessageNotificationWorkerTest {

    @Test
    fun `WHEN fetch and apply operations complete within the timeout THEN tryFetchAndApplyNimbusMessages returns true`() =
        runTest {
            val fetchExperimentDelayMillis = 100L
            val applyExperimentDelayMillis = 100L
            val operationTimeout = 300L

            val nimbus = FakeNimbus(
                coroutineScope = this,
                fetchExperimentDelayMillis = fetchExperimentDelayMillis,
                applyExperimentDelayMillis = applyExperimentDelayMillis,
            )
            val experimentsFetchedOperation = CompletableDeferred<Unit>()
            val experimentsAppliedOperation = CompletableDeferred<Unit>()

            assertNoOperationsStarted(
                nimbus,
                experimentsFetchedOperation,
                experimentsAppliedOperation,
            )

            val result = tryFetchAndApplyNimbusExperiments(
                nimbusSdk = nimbus,
                operationTimeout = operationTimeout,
                experimentsFetched = experimentsFetchedOperation,
                experimentsApplied = experimentsAppliedOperation,
            )

            assertTrue(nimbus.wasRegistered)
            assertTrue(nimbus.fetchExperimentsWasCalled)
            assertTrue(nimbus.applyPendingExperimentsWasCalled)
            assertTrue(experimentsFetchedOperation.isCompleted)
            assertTrue(experimentsAppliedOperation.isCompleted)
            assertTrue(nimbus.wasUnregistered)
            assertTrue(result)
        }

    @Test
    fun `WHEN fetch operation does not complete within the timeout THEN tryFetchAndApplyNimbusMessages returns false`() =
        runTest {
            val fetchExperimentDelayMillis = 400L
            val operationTimeout = 300L

            val nimbus = FakeNimbus(
                coroutineScope = this,
                fetchExperimentDelayMillis = fetchExperimentDelayMillis,
            )
            val experimentsFetchedOperation = CompletableDeferred<Unit>()
            val experimentsAppliedOperation = CompletableDeferred<Unit>()

            assertNoOperationsStarted(
                nimbus,
                experimentsFetchedOperation,
                experimentsAppliedOperation,
            )

            val result = tryFetchAndApplyNimbusExperiments(
                nimbusSdk = nimbus,
                operationTimeout = operationTimeout,
                experimentsFetched = experimentsFetchedOperation,
                experimentsApplied = experimentsAppliedOperation,
            )

            assertTrue(nimbus.wasRegistered)
            assertTrue(nimbus.fetchExperimentsWasCalled)
            assertFalse(nimbus.applyPendingExperimentsWasCalled)
            assertTrue(experimentsFetchedOperation.isCompleted)
            assertTrue(experimentsAppliedOperation.isCompleted)
            assertTrue(nimbus.wasUnregistered)
            assertFalse(result)
        }

    @Test
    fun `WHEN apply operation does not complete within the timeout THEN tryFetchAndApplyNimbusMessages returns false`() =
        runTest {
            val fetchExperimentDelayMillis = 100L
            val applyExperimentDelayMillis = 400L
            val operationTimeout = 300L

            val nimbus = FakeNimbus(
                coroutineScope = this,
                fetchExperimentDelayMillis = fetchExperimentDelayMillis,
                applyExperimentDelayMillis = applyExperimentDelayMillis,
            )
            val experimentsFetchedOperation = CompletableDeferred<Unit>()
            val experimentsAppliedOperation = CompletableDeferred<Unit>()

            assertNoOperationsStarted(
                nimbus,
                experimentsFetchedOperation,
                experimentsAppliedOperation,
            )

            val result = tryFetchAndApplyNimbusExperiments(
                nimbusSdk = nimbus,
                operationTimeout = operationTimeout,
                experimentsFetched = experimentsFetchedOperation,
                experimentsApplied = experimentsAppliedOperation,
            )

            assertTrue(nimbus.wasRegistered)
            assertTrue(nimbus.fetchExperimentsWasCalled)
            assertTrue(nimbus.applyPendingExperimentsWasCalled)
            assertTrue(experimentsFetchedOperation.isCompleted)
            assertTrue(experimentsAppliedOperation.isCompleted)
            assertTrue(nimbus.wasUnregistered)
            assertFalse(result)
        }

    private fun assertNoOperationsStarted(
        nimbus: FakeNimbus,
        experimentsFetchedOperation: CompletableDeferred<Unit>,
        experimentsAppliedOperation: CompletableDeferred<Unit>,
    ) {
        assertFalse(nimbus.wasRegistered)
        assertFalse(nimbus.fetchExperimentsWasCalled)
        assertFalse(nimbus.applyPendingExperimentsWasCalled)
        assertFalse(experimentsFetchedOperation.isCompleted)
        assertFalse(experimentsAppliedOperation.isCompleted)
        assertFalse(nimbus.wasUnregistered)
    }
}

private class FakeNimbus(
    private val coroutineScope: CoroutineScope,
    private val fetchExperimentDelayMillis: Long = 0L,
    private val applyExperimentDelayMillis: Long = 0L,
) : NimbusApi {
    var wasRegistered = false
    var wasUnregistered = false
    var fetchExperimentsWasCalled = false
    var applyPendingExperimentsWasCalled = false

    private lateinit var observer: NimbusInterface.Observer

    override val context = testContext

    override fun register(observer: NimbusInterface.Observer) {
        this.observer = observer
        wasRegistered = true
    }

    override fun unregister(observer: NimbusInterface.Observer) {
        wasUnregistered = true
    }

    override fun fetchExperiments() {
        fetchExperimentsWasCalled = true
        coroutineScope.launch {
            delay(fetchExperimentDelayMillis) // simulate async fetch.
            observer.onExperimentsFetched()
        }
    }

    override fun applyPendingExperiments(): Job {
        applyPendingExperimentsWasCalled = true
        return coroutineScope.launch {
            delay(applyExperimentDelayMillis) // simulate async apply.
            observer.onUpdatesApplied(emptyList())
        }
    }

    // Remaining methods are unused in this context and can be left unimplemented.
    override fun register(
        observer: NimbusInterface.Observer,
        owner: LifecycleOwner,
        autoPause: Boolean,
    ) = Unit

    override fun register(observer: NimbusInterface.Observer, view: View) = Unit
    override fun unregisterObservers() = Unit
    override fun notifyObservers(block: NimbusInterface.Observer.() -> Unit) = Unit
    override fun notifyAtLeastOneObserver(block: NimbusInterface.Observer.() -> Unit) = Unit
    override fun pauseObserver(observer: NimbusInterface.Observer) = Unit
    override fun resumeObserver(observer: NimbusInterface.Observer) = Unit
    override fun <R> wrapConsumers(block: NimbusInterface.Observer.(R) -> Boolean): List<(R) -> Boolean> =
        emptyList()

    override fun isObserved(): Boolean = false
}
