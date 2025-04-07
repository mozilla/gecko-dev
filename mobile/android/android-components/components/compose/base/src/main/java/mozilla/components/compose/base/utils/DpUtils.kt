/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.base.utils

import androidx.compose.ui.unit.Dp

/**
 * Extension function for calculating the fraction of a measurement in Dp against a total width
 * returned in float value (0f-1f).
 *
 * @param totalWidth the denominator in the fraction.
 */
fun Dp.toFraction(totalWidth: Dp): Float {
    return (this / totalWidth).coerceIn(0f, 1f)
}
