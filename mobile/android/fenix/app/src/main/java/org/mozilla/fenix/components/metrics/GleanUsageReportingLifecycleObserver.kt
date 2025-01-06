/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.LifecycleOwner
import org.mozilla.fenix.GleanMetrics.Pings
import org.mozilla.fenix.GleanMetrics.Usage

internal class GleanUsageReportingLifecycleObserver : LifecycleEventObserver {
    /**
     * Called when lifecycle events are triggered.
     */
    override fun onStateChanged(source: LifecycleOwner, event: Lifecycle.Event) {
        when (event) {
            Lifecycle.Event.ON_START -> {
                Usage.reason.set("active")
                Pings.usageReporting.submit()
            }
            Lifecycle.Event.ON_STOP -> {
                Usage.reason.set("inactive")
                Pings.usageReporting.submit()
            }
            else -> {
                // For other lifecycle events, do nothing
            }
        }
    }
}
