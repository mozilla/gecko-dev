/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils.ext

import androidx.core.graphics.Insets
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsCompat.Type.displayCutout
import androidx.core.view.WindowInsetsCompat.Type.systemBars

/**
 * Returns the top system window inset in pixels.
 */
fun WindowInsetsCompat.top(): Int =
    this.getInsetsIgnoringVisibility(systemBars() or displayCutout()).top

/**
 * Returns the right system window inset in pixels.
 */
fun WindowInsetsCompat.right(): Int =
    this.getInsetsIgnoringVisibility(systemBars() or displayCutout()).right

/**
 * Returns the left system window inset in pixels.
 */
fun WindowInsetsCompat.left(): Int =
    this.getInsetsIgnoringVisibility(systemBars() or displayCutout()).left

/**
 * Returns the bottom system window inset in pixels.
 */
fun WindowInsetsCompat.bottom(): Int =
    this.getInsetsIgnoringVisibility(systemBars() or displayCutout()).bottom

/**
 * Returns the mandatory system gesture insets.
 */
fun WindowInsetsCompat.mandatorySystemGestureInsets(): Insets =
    this.getInsets(WindowInsetsCompat.Type.mandatorySystemGestures())
