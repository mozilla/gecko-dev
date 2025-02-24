/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import android.util.Log
import androidx.test.espresso.IdlingResource
import org.mozilla.fenix.helpers.Constants.TAG

class TimerIdlingResource(private val timeout: Long) : IdlingResource {

    private var startTime: Long = 0
    private var resourceCallback: IdlingResource.ResourceCallback? = null

    // Return the name of the IdlingResource
    override fun getName(): String = TimerIdlingResource::class.java.name

    // Check if the IdlingResource is idle
    override fun isIdleNow(): Boolean {
        // Calculate the elapsed time
        val elapsed = System.currentTimeMillis() - startTime
        // Determine if the elapsed time is greater than or equal to the timeout
        val idle = elapsed >= timeout
        Log.i(TAG, "TimerIdlingResource: Checking if the resource is idle. Elapsed time: $elapsed ms, Timeout: $timeout ms, Is idle: $idle")
        // If idle, notify the callback
        if (idle) {
            resourceCallback?.onTransitionToIdle()
            Log.i(TAG, "TimerIdlingResource: The resource transitioned to idle")
        }
        return idle
    }

    // Register the callback to be notified when the resource transitions to idle
    override fun registerIdleTransitionCallback(callback: IdlingResource.ResourceCallback?) {
        resourceCallback = callback
        // Reset the start time
        startTime = System.currentTimeMillis()
        Log.i(TAG, "SessionLoadedIdlingResource: Trying to verify that the resource transitioned from busy to idle")
    }
}
