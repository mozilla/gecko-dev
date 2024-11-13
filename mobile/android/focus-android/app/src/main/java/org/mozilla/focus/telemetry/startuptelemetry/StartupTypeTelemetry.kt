/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.telemetry.startuptelemetry

import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.Companion.NONE
import androidx.annotation.VisibleForTesting.Companion.PRIVATE
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleCoroutineScope
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.focus.GleanMetrics.PerfStartup
import org.mozilla.focus.activity.MainActivity
import org.mozilla.focus.telemetry.startuptelemetry.StartupPathProvider.StartupPath
import org.mozilla.focus.telemetry.startuptelemetry.StartupStateProvider.StartupState

private val activityClass = MainActivity::class.java

private val logger = Logger("StartupTypeTelemetry")

/**
 * Records telemetry for the number of start ups. See the
 * [Fenix perf glossary](https://wiki.mozilla.org/index.php?title=Performance/Fenix/Glossary)
 * for specific definitions.
 *
 * This should be a member variable of [MainActivity] because its data is tied to the lifecycle of an
 * Activity. Call [attachOnMainActivityOnCreate] for this class to work correctly.
 *
 * N.B.: this class is lightly hardcoded to MainActivity.
 * @param startupStateProvider Provides the startup state for the activity.
 * @param startupPathProvider Provides the startup path for the activity.
 */
class StartupTypeTelemetry(
    private val startupStateProvider: StartupStateProvider,
    private val startupPathProvider: StartupPathProvider,
) {

    /**
     * Attaches the lifecycle observer to the MainActivity's lifecycle.
     *
     * @param lifecycle The lifecycle of the MainActivity.
     */
    fun attachOnMainActivityOnCreate(lifecycle: Lifecycle) {
        lifecycle.addObserver(StartupTypeLifecycleObserver())
    }

    /**
     * Provides the label for startup telemetry based on the startup state and path.
     *
     * @return The startup telemetry label.
     */
    private fun getStartupTelemetryLabel(): String {
        val startupState = startupStateProvider.getStartupStateForStartedActivity(activityClass)
        val startupPath = startupPathProvider.startupPathForActivity
        // We don't use the enum name directly to avoid unintentional changes when refactoring.
        val stateLabel = when (startupState) {
            StartupState.COLD -> "cold"
            StartupState.WARM -> "warm"
            StartupState.HOT -> "hot"
            StartupState.UNKNOWN -> "unknown"
        }

        val pathLabel = when (startupPath) {
            StartupPath.MAIN -> "main"
            StartupPath.VIEW -> "view"

            // To avoid combinatorial explosion in label names, we bucket NOT_SET into UNKNOWN.
            StartupPath.NOT_SET,
            StartupPath.UNKNOWN,
            -> "unknown"
        }

        return "${stateLabel}_$pathLabel"
    }

    /**
     * Provides an instance of `StartupTypeLifecycleObserver` for testing purposes.
     *
     * @return A new instance of `StartupTypeLifecycleObserver`.
     */
    @VisibleForTesting(otherwise = NONE)
    fun getTestCallbacks() = StartupTypeLifecycleObserver()

    /**
     * Records startup telemetry based on the available [startupStateProvider] and [startupPathProvider].
     *
     * @param owner The [LifecycleOwner] whose [LifecycleCoroutineScope] is to be used for recording telemetry.
     * @param dispatcher The dispatcher used to control the thread on which telemetry will be recorded.
     * Defaults to [Dispatchers.IO].
     */
    @VisibleForTesting(otherwise = PRIVATE)
    fun recordStartupTelemetry(
        owner: LifecycleOwner,
        dispatcher: CoroutineDispatcher = Dispatchers.IO,
    ) {
        val label = getStartupTelemetryLabel()
        val scope = getScope(owner)

        scope.launch(dispatcher) {
            PerfStartup.startupType[label].add(1)
            logger.info("Recorded start up: $label")
        }
    }

    /**
     * Retrieves the `CoroutineScope` associated with the given `LifecycleOwner`.
     *
     * @param owner The [LifecycleOwner] whose [LifecycleCoroutineScope] is to be retrieved.
     * @return The [LifecycleCoroutineScope] associated with the given [LifecycleOwner].
     */
    internal fun getScope(owner: LifecycleOwner): CoroutineScope {
        return owner.lifecycleScope
    }

    /**
     * Lifecycle observer that records startup telemetry when the activity starts and resumes.
     */
    @VisibleForTesting(otherwise = PRIVATE)
    inner class StartupTypeLifecycleObserver : DefaultLifecycleObserver {
        private var shouldRecordStart = false

        override fun onStart(owner: LifecycleOwner) {
            shouldRecordStart = true
        }

        override fun onResume(owner: LifecycleOwner) {
            // We must record in onResume because the StartupStateProvider can only be called for
            // STARTED activities and we can't guarantee our onStart is called before its.
            //
            // We only record if start was called for this resume to avoid recording
            // for onPause -> onResume states.
            if (shouldRecordStart) {
                recordStartupTelemetry(owner)
                shouldRecordStart = false
            }
        }
    }
}
