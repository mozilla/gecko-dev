/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import androidx.compose.animation.EnterTransition
import androidx.compose.animation.ExitTransition
import androidx.compose.animation.SizeTransform
import androidx.compose.animation.core.Easing
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.FiniteAnimationSpec
import androidx.compose.animation.core.keyframes
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.ui.unit.IntSize

const val DURATION_MS_SUB_MENU = 150
const val DELAY_MS_SUB_MENU = DURATION_MS_SUB_MENU - 10
const val DURATION_MS_MAIN_MENU = 75
const val DELAY_MS_MAIN_MENU = DURATION_MS_MAIN_MENU - 5
const val DURATION_MS_TRANSLATIONS = 140
const val DELAY_MS_TRANSLATIONS = DURATION_MS_TRANSLATIONS - 10

/**
 * [EnterTransition] when navigating to the main content sheet.
 */
fun enterMenu(duration: Int, delay: Int, easing: Easing = FastOutSlowInEasing): EnterTransition {
    return fadeIn(
        animationSpec = tween(
            durationMillis = duration,
            delayMillis = duration + delay,
            easing = easing,
        ),
    ) + slideInHorizontally(
        initialOffsetX = { fullWidth -> -fullWidth / 8 },
        animationSpec = tween(
            durationMillis = duration,
            delayMillis = duration + delay,
            easing = easing,
        ),
    )
}

/**
 * [ExitTransition] when navigating from the main content sheet.
 */
fun exitMenu(duration: Int, easing: Easing = FastOutSlowInEasing): ExitTransition {
    return fadeOut(
        animationSpec = tween(
            durationMillis = duration,
            easing = easing,
        ),
    ) + slideOutHorizontally(
        targetOffsetX = { fullWidth -> -fullWidth / 8 },
        animationSpec = tween(
            durationMillis = duration,
            easing = easing,
        ),
    )
}

/**
 * [EnterTransition] when navigating to the sub menu content sheet.
 */
fun enterSubmenu(duration: Int, delay: Int, easing: Easing = FastOutSlowInEasing): EnterTransition {
    return fadeIn(
        animationSpec = tween(
            durationMillis = duration,
            delayMillis = duration + delay,
            easing = easing,
        ),
    ) + slideInHorizontally(
        initialOffsetX = { fullWidth -> fullWidth / 8 },
        animationSpec = tween(
            durationMillis = duration,
            delayMillis = duration + delay,
            easing = easing,
        ),
    )
}

/**
 * [ExitTransition] when navigating from sub menu content sheet.
 */
fun exitSubmenu(duration: Int, easing: Easing = FastOutSlowInEasing): ExitTransition {
    return fadeOut(
        animationSpec = tween(
            durationMillis = duration,
            easing = easing,
        ),
    ) + slideOutHorizontally(
        targetOffsetX = { fullWidth -> fullWidth / 8 },
        animationSpec = tween(
            durationMillis = duration,
            easing = easing,
        ),
    )
}

/**
 * For use with [SizeTransform] to specify how the content should grow based on changes.
 */
fun contentGrowth(
    initialSize: IntSize,
    targetSize: IntSize,
    duration: Int,
): FiniteAnimationSpec<IntSize> {
    return keyframes {
        // Time the total animations should play
        durationMillis = duration * 2
        // Stay at the same size for the first duration
        IntSize(targetSize.width, initialSize.height) at duration
        // At the next duration, grow to full height.
        IntSize(targetSize.width, targetSize.height) at duration * 2
    }
}
