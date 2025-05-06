/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.about

import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleOwner

/**
 * Triggers the "secret" debug menu when logoView is tapped 5 times.
 */
class SecretDebugMenuTrigger(
    private val onLogoClicked: (Int) -> Unit,
    private val onDebugMenuActivated: () -> Unit,
) : DefaultLifecycleObserver {

    private var secretDebugMenuClicks = 0

    /**
     * Reset the [secretDebugMenuClicks] counter.
     */
    override fun onResume(owner: LifecycleOwner) {
        secretDebugMenuClicks = 0
    }

    internal fun onClick() {
        secretDebugMenuClicks += 1
        when (secretDebugMenuClicks) {
            in 2 until SECRET_DEBUG_MENU_CLICKS -> {
                val clicksLeft = SECRET_DEBUG_MENU_CLICKS - secretDebugMenuClicks
                onLogoClicked(clicksLeft)
            }
            SECRET_DEBUG_MENU_CLICKS -> {
                onDebugMenuActivated()
            }
        }
    }

    companion object {
        // Number of clicks on the app logo to enable the "secret" debug menu.
        private const val SECRET_DEBUG_MENU_CLICKS = 5
    }
}
