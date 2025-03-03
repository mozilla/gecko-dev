/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.ui.theme

import androidx.compose.foundation.background
import androidx.compose.ui.Modifier
import androidx.compose.ui.composed
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.res.colorResource
import org.mozilla.focus.R

/**
 * Creates a linear gradient using six predefined colors from the application's
 * resources and applies it as the background of the composable.
 *
 * The gradient colors are:
 * - `home_screen_modal_gradient_one`
 * - `home_screen_modal_gradient_two`
 * - `home_screen_modal_gradient_three`
 * - `home_screen_modal_gradient_four`
 * - `home_screen_modal_gradient_five`
 * - `home_screen_modal_gradient_six`
 *
 * These colors are currently defined in the application's `colors.xml` resource file.
 *
 * The gradient starts at the top-right corner (x = infinity, y = 0) and ends at the
 * bottom-left corner (x = 0, y = infinity). This ensures that the gradient covers
 * the entire composable area regardless of its dimensions.
 *
 * @return A [Modifier] that applies the linear gradient background.
 *
 * Example Usage:
 * ```
 * Box(modifier = Modifier
 *     .fillMaxSize()
 *     .gradientBackground()
 * ) {
 *     // Content of the box
 * }
 * ```
 */
fun Modifier.gradientBackground() = composed {
    val gradient = Brush.linearGradient(
        colors = listOf(
            colorResource(R.color.home_screen_modal_gradient_one),
            colorResource(R.color.home_screen_modal_gradient_two),
            colorResource(R.color.home_screen_modal_gradient_three),
            colorResource(R.color.home_screen_modal_gradient_four),
            colorResource(R.color.home_screen_modal_gradient_five),
            colorResource(R.color.home_screen_modal_gradient_six),
        ),
        end = Offset(0f, Float.POSITIVE_INFINITY),
        start = Offset(Float.POSITIVE_INFINITY, 0f),
    )
    background(brush = gradient)
}
