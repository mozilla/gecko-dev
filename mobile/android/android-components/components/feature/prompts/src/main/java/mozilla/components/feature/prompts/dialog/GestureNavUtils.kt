/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.dialog

import android.os.Build
import android.view.Window
import androidx.core.view.WindowInsetsCompat

/**
 * A place to keep utility functions for gesture navigation.
 */
object GestureNavUtils {
    /**
     * Find out if the device supports gesture navigation and has gestures enabled.
     *
     * Gesture nav support was added in Android Q, but on recent devices it is possible to
     * switch navigation mode between gesture nav and 3-button navigation.
     * The system gesture inset needs to be greater than 0 at the left edge of the screen
     * for the back gesture to be picked up, so this seems to be the most reliable way of
     * detecting gesture navigation use.
     *
     * * See also: [Gesture Navigation: handling visual overlaps (II)](https://medium.com/androiddevelopers/gesture-navigation-handling-visual-overlaps-4aed565c134c)
     * * See also: [How to detect full screen gesture mode](https://stackoverflow.com/a/70514883)
     *
     * @param window The containing [Window] of the current `Activity`.
     * @return true if the device supports gesture navigation and gestures are enabled.
     */
    fun isInGestureNavigationMode(window: Window): Boolean =
        (
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                WindowInsetsCompat
                    .toWindowInsetsCompat(
                        window.decorView.rootWindowInsets,
                    ).getInsets(WindowInsetsCompat.Type.systemGestures())
                    .left
            } else {
                0
            }
            ) > 0
}
