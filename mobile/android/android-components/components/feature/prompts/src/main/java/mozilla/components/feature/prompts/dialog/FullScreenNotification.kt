/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.dialog

import android.app.Activity
import android.widget.Toast

/**
 * UI to show a 'full screen mode' notification.
 */
interface FullScreenNotification {
    /**
     * Show the notification.
     */
    fun show()
}

/**
 * A [Toast] to show a full screen notification message
 * @property activity The activity to show the toast on.
 * @property gestureNavString The string to show when in gesture navigation mode.
 * @property backButtonString The string to show when in 3-button navigation mode.
 * @property gestureNavUtils Utility to detect gesture navigation.
 */
class FullScreenNotificationToast(
    private val activity: Activity,
    private val gestureNavString: String,
    private val backButtonString: String,
    private val gestureNavUtils: GestureNavUtils,
) : FullScreenNotification {
    override fun show() {
        val toastString =
            if (gestureNavUtils.isInGestureNavigationMode(activity.window)) {
                gestureNavString
            } else {
                backButtonString
            }
        Toast.makeText(activity, toastString, Toast.LENGTH_LONG).show()
    }
}
