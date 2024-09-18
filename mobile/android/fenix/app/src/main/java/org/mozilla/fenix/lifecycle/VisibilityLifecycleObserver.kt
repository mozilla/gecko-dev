/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.lifecycle

import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.LifecycleOwner
import org.mozilla.fenix.BiometricAuthenticationManager

/**
 * [LifecycleObserver] to keep track of application visibility.
 */
class VisibilityLifecycleObserver : DefaultLifecycleObserver {
    override fun onPause(owner: LifecycleOwner) {
        super.onPause(owner)
        BiometricAuthenticationManager.biometricAuthenticationNeededInfo.shouldAuthenticate = true
    }
}
