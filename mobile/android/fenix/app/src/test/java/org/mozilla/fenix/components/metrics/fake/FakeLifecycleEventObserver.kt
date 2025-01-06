/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics.fake

import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.LifecycleOwner

/**
 * Allows tests to insert their own version of a LifecycleEventObserver
 * and make assertions against it
 */
class FakeLifecycleEventObserver : LifecycleEventObserver {
    var lastEvent: Lifecycle.Event? = null
    override fun onStateChanged(source: LifecycleOwner, event: Lifecycle.Event) {
        lastEvent = event
    }
}
