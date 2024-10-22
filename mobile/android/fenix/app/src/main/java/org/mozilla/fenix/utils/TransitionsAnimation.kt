/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import androidx.compose.animation.EnterTransition
import androidx.compose.animation.ExitTransition
import androidx.compose.animation.SizeTransform
import androidx.compose.animation.core.FiniteAnimationSpec
import androidx.compose.animation.core.keyframes
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.ui.unit.IntSize

private const val DURATION_MS = 140
private const val DELAY_MS = DURATION_MS - 10

/**
 * [EnterTransition] when navigating to the main content sheet.
 */
fun enterMenu(): EnterTransition {
    return fadeIn(
        animationSpec = tween(durationMillis = DURATION_MS, delayMillis = DURATION_MS + DELAY_MS),
    ) + slideInHorizontally(
        initialOffsetX = { fullWidth -> -fullWidth / 8 },
        animationSpec = tween(durationMillis = DURATION_MS, delayMillis = DURATION_MS + DELAY_MS),
    )
}

/**
 * [ExitTransition] when navigating from the main content sheet.
 */
fun exitMenu(): ExitTransition {
    return fadeOut(
        animationSpec = tween(durationMillis = DURATION_MS),
    ) + slideOutHorizontally(
        targetOffsetX = { fullWidth -> -fullWidth / 8 },
        animationSpec = tween(durationMillis = DURATION_MS),
    )
}

/**
 * [EnterTransition] when navigating to the sub menu content sheet.
 */
fun enterSubmenu(): EnterTransition {
    return fadeIn(
        animationSpec = tween(durationMillis = DURATION_MS, delayMillis = DURATION_MS + DELAY_MS),
    ) + slideInHorizontally(
        initialOffsetX = { fullWidth -> fullWidth / 8 },
        animationSpec = tween(durationMillis = DURATION_MS, delayMillis = DURATION_MS + DELAY_MS),
    )
}

/**
 * [ExitTransition] when navigating from sub menu content sheet.
 */
fun exitSubmenu(): ExitTransition {
    return fadeOut(
        animationSpec = tween(durationMillis = DURATION_MS),
    ) + slideOutHorizontally(
        targetOffsetX = { fullWidth -> fullWidth / 8 },
        animationSpec = tween(durationMillis = DURATION_MS),
    )
}

/**
 * For use with [SizeTransform] to specify how the content should grow based on changes.
 */
fun contentGrowth(initialSize: IntSize, targetSize: IntSize): FiniteAnimationSpec<IntSize> {
    return keyframes {
        // Time the total animations should play
        durationMillis = DURATION_MS * 2
        // Stay at the same size for the first duration
        IntSize(targetSize.width, initialSize.height) at DURATION_MS
        // At the next duration, grow to full height.
        IntSize(targetSize.width, targetSize.height) at DURATION_MS * 2
    }
}
