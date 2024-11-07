/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import android.view.View
import android.view.animation.AlphaAnimation
import android.view.animation.Animation
import android.view.animation.Animation.AnimationListener
import android.view.animation.AnimationSet
import android.view.animation.TranslateAnimation
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
import androidx.fragment.app.Fragment
import com.google.android.material.bottomsheet.BottomSheetDialogFragment

const val DURATION_MS_SUB_MENU = 150
const val DELAY_MS_SUB_MENU = DURATION_MS_SUB_MENU - 10
const val DURATION_MS_MAIN_MENU = 75
const val DELAY_MS_MAIN_MENU = DURATION_MS_MAIN_MENU - 5
const val DURATION_MS_TRANSLATIONS = 140
const val DELAY_MS_TRANSLATIONS = DURATION_MS_TRANSLATIONS - 10
private const val SLIDE_DOWN_ANIMATION_DURATION_MS = 150L
private const val SLIDE_DOWN_ANIMATION_ALPHA_DURATION_MS = 100L

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

/**
 * Fragment animation when navigating a [BottomSheetDialogFragment] to other [Fragment].
 */
@Suppress("EmptyFunctionBlock")
fun View.slideDown(
    endOfAnimationCallback: () -> Unit,
) {
    val animationSet = AnimationSet(false)

    val translateAnimation = TranslateAnimation(0f, 0f, 0f, this.height.toFloat())

    translateAnimation.duration = SLIDE_DOWN_ANIMATION_DURATION_MS

    val alphaAnimation = AlphaAnimation(1f, 0f)
    alphaAnimation.duration = SLIDE_DOWN_ANIMATION_ALPHA_DURATION_MS

    animationSet.addAnimation(translateAnimation)
    animationSet.addAnimation(alphaAnimation)
    animationSet.setAnimationListener(
        object : AnimationListener {
            override fun onAnimationStart(p0: Animation?) {}

            override fun onAnimationEnd(p0: Animation?) {
                endOfAnimationCallback()
            }

            override fun onAnimationRepeat(p0: Animation?) {}
        },
    )
    this.startAnimation(animationSet)
}
