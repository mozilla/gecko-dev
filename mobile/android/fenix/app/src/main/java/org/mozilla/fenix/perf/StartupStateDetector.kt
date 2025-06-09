/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import android.app.Activity
import android.app.Application
import android.os.Bundle
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.ProcessLifecycleOwner
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.android.DefaultActivityLifecycleCallbacks

/**
 * Detects the [StartupState] associated with app startup. This interface just defines the
 * API contract.
 *
 * See [DefaultStartupStateDetector] for the implementation details of how [StartupState] is determined
 */
interface StartupStateDetector {

    /**
     * Registers the [StartupStateDetector] in application's onCreate method. This needs to be called
     * before [getStartupState]
     *
     * @param application] [Application] to set up the lifecycle callbacks used to determine [StartupState]
     */
    fun registerInAppOnCreate(application: Application)

    /**
     * @return the [StartupState] based on the heuristics we have defined.
     */
    fun getStartupState(): StartupState
}

/**
 * Detects the [StartupState]. This class uses a combination of [androidx.lifecycle.ProcessLifecycleOwner]
 * and [android.app.Application.ActivityLifecycleCallbacks] to determine an approximate [StartupState]
 *
 * This class relies completely on [HomeActivity] as the activity we use to determine the [StartupState].
 *
 * See [Fenix perf glossary](https://wiki.mozilla.org/index.php?title=Performance/Fenix/Glossary)
 * for specific definitions.
 *
 * Specifically, if:
 * 1. Warm start is determined if onCreate() is called with a saved state
 * 2. Hot start is determined if the activity goes to the stopped state, and then goes back into the started state
 * 3. Cold start is when the process AND the [HomeActivity] are created,
 * and neither warm, hot, nor cold start is detected,
 */
internal class DefaultStartupStateDetector : StartupStateDetector {

    private val processLifecycleObserver = ProcessLifecycleObserver()
    private val activityLifecycleListener = ActivityLifecycleCallbacksListener()

    /**
     * Registers the [StartupStateDetector] in application's onCreate method. This needs to be called
     * before [getStartupState]
     *
     * @param application] [Application] to set up the lifecycle callbacks used to determine [StartupState]
     */
    override fun registerInAppOnCreate(application: Application) {
        ProcessLifecycleOwner.get().lifecycle.addObserver(processLifecycleObserver)
        application.registerActivityLifecycleCallbacks(activityLifecycleListener)
    }

    /**
     * @return the [StartupState] based on the heuristics we have defined.
     */
    override fun getStartupState(): StartupState {
        val startupState = when {
            // in the case where there was previously saved state, and we do a "hot" restart, (e.g
            // by navigating to another activity and back, the saved state bundle is still there
            // and we might wrongly attribute that as a "warm start", when in fact, it's a "hot" one
            activityLifecycleListener.hasSavedState && !activityLifecycleListener.isRestarted -> StartupState.WARM
            activityLifecycleListener.isRestarted -> StartupState.HOT
            processLifecycleObserver.isCreated && activityLifecycleListener.isCreated -> StartupState.COLD
            else -> StartupState.UNKNOWN
        }

        return startupState
    }

    /**
     * Class to listen for the process created and destroy events - used to determine startup type
     */
    private class ProcessLifecycleObserver : DefaultLifecycleObserver {

        /**
         * Indicates whether or not the process is created
         */
        var isCreated = false
            private set

        /**
         * Called when the process [DefaultLifecycleObserver] is or has been created
         */
        override fun onCreate(owner: LifecycleOwner) {
            isCreated = true
        }

        /**
         * Called when the process [DefaultLifecycleObserver] is destroyed
         */
        override fun onDestroy(owner: LifecycleOwner) {
            isCreated = false
        }
    }

    private class ActivityLifecycleCallbacksListener : DefaultActivityLifecycleCallbacks {

        var hasSavedState = false
            private set
        var isCreated = false
            private set
        var isStopped = false
            private set
        var isRestarted = false
            private set

        private val homeActivityKey = HomeActivity::class.java.name

        override fun onActivityCreated(activity: Activity, bundle: Bundle?) {
            if (activity.isNotHomeActivity()) return

            hasSavedState = bundle != null
            isCreated = true
        }

        override fun onActivityStarted(activity: Activity) {
            if (activity.isNotHomeActivity()) return

            // if we previously stopped the home activity and started it, we should treat it as a restart
            isRestarted = isStopped

            // reset the "stopped" state
            // so that if onStart gets called again, we won't treat it as a restart
            isStopped = false
        }

        override fun onActivityStopped(activity: Activity) {
            if (activity.isNotHomeActivity()) return
            isStopped = true
        }

        override fun onActivityDestroyed(activity: Activity) {
            if (activity.isNotHomeActivity()) return
            resetAllStates()
        }

        private fun resetAllStates() {
            isStopped = false
            isRestarted = false
            isCreated = false
        }

        private fun Activity.isNotHomeActivity(): Boolean {
            return this::class.java.name != homeActivityKey
        }
    }
}
