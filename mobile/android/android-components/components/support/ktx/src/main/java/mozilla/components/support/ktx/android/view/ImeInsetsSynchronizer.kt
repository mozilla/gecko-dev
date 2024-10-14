/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.view

import android.os.Build
import android.view.View
import android.view.ViewGroup
import androidx.core.view.OnApplyWindowInsetsListener
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsAnimationCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsCompat.Type.ime
import androidx.core.view.WindowInsetsCompat.Type.systemBars

/**
 * Helper class allowing to easily synchronize a [View] with the keyboard insets leading to a
 * smooth animation for both of these when showing or hiding the keyboard.
 *
 * This only handles the IME insets and allows for a smooth resizing animation while it
 * assumes that persistent insets - for system bars are handled separately.
 *
 * @param targetView The view which will be shown on top of the keyboard while this is animated to be
 * showing or to be hidden.
 * @param synchronizeViewWithIME Whether to automatically apply the needed margins to [targetView]
 * to ensure it will be animated together with the keyboard or not. As an alternative integrators can use
 * the [onIMEAnimationStarted] and [onIMEAnimationFinished] callbacks to resize the layout on their own.
 * @param onIMEAnimationStarted Callback for when the IME animation starts.
 * It will inform whether the keyboard is showing or hiding and the height of the keyboard.
 * @param onIMEAnimationFinished Callback for when the IME animation finishes.
 * It will inform whether the keyboard is showing or hiding and the height of the keyboard.
 */
class ImeInsetsSynchronizer private constructor(
    private val targetView: View,
    private val synchronizeViewWithIME: Boolean,
    private val onIMEAnimationStarted: (Boolean, Int) -> Unit,
    private val onIMEAnimationFinished: (Boolean, Int) -> Unit,
) : WindowInsetsAnimationCompat.Callback(DISPATCH_MODE_CONTINUE_ON_SUBTREE),
    OnApplyWindowInsetsListener {

    init {
        ViewCompat.setWindowInsetsAnimationCallback(targetView, this)
        ViewCompat.setOnApplyWindowInsetsListener(targetView, this)
    }

    private lateinit var lastWindowInsets: WindowInsetsCompat
    private var areKeyboardInsetsDeferred = false
    private var isKeyboardShowingUp: Boolean = true
    private var keyboardHeight = 0

    override fun onApplyWindowInsets(
        view: View,
        windowInsets: WindowInsetsCompat,
    ): WindowInsetsCompat {
        lastWindowInsets = windowInsets
        isKeyboardShowingUp = windowInsets.isKeyboardShowingUp

        if (!areKeyboardInsetsDeferred) {
            view.updateBottomMargin(
                calculateBottomMargin(
                    windowInsets.keyboardInsets.bottom,
                    getNavbarHeight(),
                ),
            )

            onIMEAnimationFinished(
                isKeyboardShowingUp,
                calculateBottomMargin(
                    windowInsets.keyboardInsets.bottom,
                    getNavbarHeight(),
                ),
            )
        }

        return windowInsets
    }

    override fun onPrepare(animation: WindowInsetsAnimationCompat) {
        if (animation.typeMask and ime() != 0) {
            // We defer the keyboard insets if the IME is currently not visible.
            // This results in only the system bars insets being applied, allowing the root view to remain
            // at it's larger size until it will have padding applied to account for the visible keyboard height.
            areKeyboardInsetsDeferred = true
        }
    }

    override fun onStart(
        animation: WindowInsetsAnimationCompat,
        bounds: WindowInsetsAnimationCompat.BoundsCompat,
    ): WindowInsetsAnimationCompat.BoundsCompat {
        if (animation.typeMask and ime() != 0) {
            // Workaround for https://issuetracker.google.com/issues/361027506
            // Compute the keyboard height based on the animation bounds.
            keyboardHeight = bounds.upperBound.bottom - bounds.lowerBound.bottom

            // Workaround for https://issuetracker.google.com/issues/369223558
            // We expect the keyboard to have a bigger height than the OS navigation bar.
            if (keyboardHeight <= getNavbarHeight()) {
                keyboardHeight = 0
            }

            onIMEAnimationStarted(
                isKeyboardShowingUp,
                calculateBottomMargin(keyboardHeight, getNavbarHeight()),
            )
        }

        return super.onStart(animation, bounds)
    }

    override fun onProgress(
        insets: WindowInsetsCompat,
        runningAnimations: List<WindowInsetsAnimationCompat>,
    ): WindowInsetsCompat {
        runningAnimations.firstOrNull { it.typeMask and ime() != 0 }?.let { imeAnimation ->
            // Ensure the IME animation fraction is growing when the keyboard is showing up
            // and shrinking otherwise.
            val imeAnimationFractionBasedOnDirection = when (isKeyboardShowingUp) {
                true -> imeAnimation.interpolatedFraction
                false -> 1 - imeAnimation.interpolatedFraction
            }

            targetView.updateBottomMargin(
                calculateBottomMargin(
                    (keyboardHeight * imeAnimationFractionBasedOnDirection).toInt(),
                    getNavbarHeight(),
                ),
            )
        }

        return insets
    }

    override fun onEnd(animation: WindowInsetsAnimationCompat) {
        if (areKeyboardInsetsDeferred && (animation.typeMask and ime()) != 0) {
            // If we deferred the IME insets and an IME animation has finished, we need to reset the flag
            areKeyboardInsetsDeferred = false

            // And finally dispatch the deferred insets to the view now.
            // Ideally we would just call view.requestApplyInsets() and let the normal dispatch
            // cycle happen, but this happens too late resulting in a visual flicker.
            // Instead we manually dispatch the most recent WindowInsets to the view.
            ViewCompat.dispatchApplyWindowInsets(targetView, lastWindowInsets)
        }
    }

    private val WindowInsetsCompat.keyboardInsets
        get() = getInsets(ime())

    private val WindowInsetsCompat.isKeyboardShowingUp
        get() = isVisible(ime())

    private val WindowInsetsCompat.navigationBarInsetHeight
        get() = when (isKeyboardShowingUp) {
            true -> getInsets(systemBars()).bottom
            false -> 0
        }

    private fun getNavbarHeight() = ViewCompat.getRootWindowInsets(targetView)
        ?.getInsets(systemBars())?.bottom ?: lastWindowInsets.navigationBarInsetHeight

    private fun calculateBottomMargin(
        keyboardHeight: Int,
        navigationBarHeight: Int,
    ) = (keyboardHeight - navigationBarHeight).coerceAtLeast(0)

    private fun View.updateBottomMargin(bottom: Int) {
        if (synchronizeViewWithIME) {
            (layoutParams as ViewGroup.MarginLayoutParams).setMargins(0, 0, 0, bottom)
            requestLayout()
        }
    }

    companion object {
        /**
         * Setup animating [targetView] as always on top of the keyboard while also respecting all system bars insets.
         * This works only on Android 10+, otherwise the dynamic padding based on the keyboard is not reliable.
         *
         * @param targetView The root view to add paddings to for accounting the visible keyboard height.
         * @param synchronizeViewWithIME Whether to automatically apply the needed margins to [targetView]
         * to ensure it will be animated together with the keyboard or not. As an alternative integrators can use
         * the [onIMEAnimationStarted] and [onIMEAnimationFinished] callbacks to resize the layout on their own.
         * @param onIMEAnimationStarted Callback for when the IME animation starts.
         * It will inform whether the keyboard is showing or hiding and the height of the keyboard.
         * @param onIMEAnimationFinished Callback for when the IME animation finishes.
         * It will inform whether the keyboard is showing or hiding and the height of the keyboard.
         */
        fun setup(
            targetView: View,
            synchronizeViewWithIME: Boolean = true,
            onIMEAnimationStarted: (Boolean, Int) -> Unit = { _, _ -> },
            onIMEAnimationFinished: (Boolean, Int) -> Unit = { _, _ -> },
        ) = when (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            true -> ImeInsetsSynchronizer(
                targetView,
                synchronizeViewWithIME,
                onIMEAnimationStarted,
                onIMEAnimationFinished,
            )
            false -> null
        }
    }
}
