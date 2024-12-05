/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.splashscreen

import androidx.annotation.VisibleForTesting
import mozilla.components.service.nimbus.NimbusApi
import org.mozilla.experiments.nimbus.NimbusInterface
import org.mozilla.experiments.nimbus.internal.EnrolledExperiment
import org.mozilla.fenix.utils.Settings
import kotlin.coroutines.Continuation
import kotlin.coroutines.resume
import kotlin.coroutines.suspendCoroutine

/**
 * An async operation performed during the splash screen.
 */
interface SplashScreenOperation {
    /**
     * The type of the splash screen operation.
     */
    val type: String

    /**
     * Indicates whether data was fetched during the operation.
     */
    val dataFetched: Boolean

    /**
     * Executes the splash screen operation.
     */
    suspend fun run()
}

/**
 * Interface for accessing the state of nimbus experiment data.
 */
interface ExperimentsOperationStorage {
    /**
     * Indicates whether Nimbus experiments have been fetched.
     */
    val nimbusExperimentsFetched: Boolean
}

/**
 * A default implementation of [ExperimentsOperationStorage].
 *
 * @property settings The settings object used to access the experiment data state.
 */
class DefaultExperimentsOperationStorage(
    val settings: Settings,
) : ExperimentsOperationStorage {
    override val nimbusExperimentsFetched
        get() = settings.nimbusExperimentsFetched
}

/**
 * Observes fetching of nimbus experiments.
 *
 * @param storage Interface for retrieving experiment state.
 * @param nimbus The Nimbus API to observe experiments.
 */
class FetchExperimentsOperation(
    private val storage: ExperimentsOperationStorage,
    private val nimbus: NimbusApi,
) : SplashScreenOperation {
    override val type = "fetch"

    override var dataFetched: Boolean = false
        private set

    @VisibleForTesting
    internal var fetchNimbusObserver: NimbusInterface.Observer? = null

    override suspend fun run() {
        suspendCoroutine { continuation ->
            if (storage.nimbusExperimentsFetched) {
                dataFetched = true
                continuation.resume(Unit)
            } else {
                fetchNimbusObserver = FetchNimbusObserver { dataFetched = true }.apply {
                    fetchContinuation = continuation
                    nimbus.register(this)
                }
            }
        }
        fetchNimbusObserver?.let { nimbus.unregister(it) }
    }
}

/**
 * Observes fetching of nimbus experiments and applies the fetched data to be used in this user session.
 *
 * @param storage Interface for retrieving experiment state.
 * @param nimbus The Nimbus API to observe and to apply experiments.
 */
class ApplyExperimentsOperation(
    private val storage: ExperimentsOperationStorage,
    private val nimbus: NimbusApi,
) : SplashScreenOperation {
    override val type = "apply"

    @VisibleForTesting
    internal var isDataApplied = false
    override var dataFetched: Boolean = false
        private set

    @VisibleForTesting
    internal var fetchNimbusObserver: NimbusInterface.Observer? = null

    @VisibleForTesting
    internal var applyNimbusObserver: NimbusInterface.Observer? = null

    override suspend fun run() {
        suspendCoroutine { continuation ->
            if (storage.nimbusExperimentsFetched) {
                dataFetched = true
                continuation.resume(Unit)
            } else {
                fetchNimbusObserver = FetchNimbusObserver { dataFetched = true }.apply {
                    fetchContinuation = continuation
                    nimbus.register(this)
                }
            }
        }

        suspendCoroutine { continuation ->
            nimbus.applyPendingExperiments()

            applyNimbusObserver = ApplyNimbusObserver { isDataApplied = true }.apply {
                applyContinuation = continuation
                nimbus.register(this)
            }
        }

        fetchNimbusObserver?.let { nimbus.unregister(it) }
        applyNimbusObserver?.let { nimbus.unregister(it) }
    }
}

@VisibleForTesting
internal class FetchNimbusObserver(
    private val onDataFetched: () -> Unit,
) : NimbusInterface.Observer {
    var fetchContinuation: Continuation<Unit>? = null

    override fun onExperimentsFetched() {
        onDataFetched()
        fetchContinuation?.resume(Unit)
        fetchContinuation = null
    }
}

@VisibleForTesting
internal class ApplyNimbusObserver(
    private val onDataApplied: () -> Unit,
) : NimbusInterface.Observer {
    var applyContinuation: Continuation<Unit>? = null

    override fun onUpdatesApplied(updated: List<EnrolledExperiment>) {
        onDataApplied()
        applyContinuation?.resume(Unit)
        applyContinuation = null
    }
}
