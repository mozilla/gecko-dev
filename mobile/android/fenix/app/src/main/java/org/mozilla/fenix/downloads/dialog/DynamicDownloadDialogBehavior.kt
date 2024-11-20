/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.dialog

import android.animation.Animator
import android.animation.AnimatorListenerAdapter
import android.animation.ValueAnimator
import android.view.View
import android.view.ViewGroup
import android.view.animation.DecelerateInterpolator
import androidx.annotation.VisibleForTesting
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.view.ViewCompat
import androidx.core.view.children
import androidx.core.view.isVisible
import mozilla.components.concept.engine.EngineView
import mozilla.components.support.ktx.android.view.findViewInHierarchy
import org.mozilla.fenix.R
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.utils.Settings
import kotlin.math.max
import kotlin.math.min

/**
 * A [CoordinatorLayout.Behavior] implementation to be used when placing [DynamicDownloadDialog]
 * at the bottom of the screen. Based off of BrowserToolbarBottomBehavior.
 *
 * This implementation will:
 * - Show/Hide the [DynamicDownloadDialog] automatically when scrolling vertically.
 * - Snap the [DynamicDownloadDialog] to be hidden or visible when the user stops scrolling.
 */

private const val SNAP_ANIMATION_DURATION = 150L
private val BOTTOM_TOOLBAR_ANCHOR_IDS = listOf(
    R.id.findInPageView,
    R.id.toolbar_navbar_container,
    R.id.toolbar,
)
private val TOP_TOOLBAR_ANCHOR_IDS = listOf(
    R.id.findInPageView,
    R.id.toolbar_navbar_container,
)

class DynamicDownloadDialogBehavior<V : View>(
    private val dynamicDownload: V,
    settings: Settings,
) : CoordinatorLayout.Behavior<V>(dynamicDownload.context, null) {

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var expanded: Boolean = true

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var shouldSnapAfterScroll: Boolean = false

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var snapAnimator: ValueAnimator = ValueAnimator()
        .apply {
            interpolator = DecelerateInterpolator()
            duration = SNAP_ANIMATION_DURATION
        }

    /**
     * Reference to [EngineView] used to check user's [android.view.MotionEvent]s.
     */
    @VisibleForTesting
    internal var engineView: EngineView? = null

    @VisibleForTesting
    internal var anchor: View? = null
    private val anchorHeight: Int
        get() = anchor?.height ?: 0
    private val possibleAnchors = when (settings.toolbarPosition) {
        ToolbarPosition.BOTTOM -> BOTTOM_TOOLBAR_ANCHOR_IDS
        ToolbarPosition.TOP -> TOP_TOOLBAR_ANCHOR_IDS
    }

    /**
     * Depending on how user's touch was consumed by EngineView / current website,
     *
     * we will animate the dynamic download notification dialog if:
     * - touches were used for zooming / panning operations in the website.
     *
     * We will do nothing if:
     * - the website is not scrollable
     * - the website handles the touch events itself through it's own touch event listeners.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal val shouldScroll: Boolean
        get() = engineView?.getInputResultDetail()?.let {
            (it.canScrollToBottom() || it.canScrollToTop())
        } ?: false

    override fun onStartNestedScroll(
        coordinatorLayout: CoordinatorLayout,
        child: V,
        directTargetChild: View,
        target: View,
        axes: Int,
        type: Int,
    ): Boolean {
        return if (shouldScroll && axes == ViewCompat.SCROLL_AXIS_VERTICAL) {
            shouldSnapAfterScroll = type == ViewCompat.TYPE_TOUCH
            snapAnimator.cancel()
            true
        } else if (engineView?.getInputResultDetail()?.isTouchUnhandled() == true) {
            // Force expand the notification dialog if event is unhandled, otherwise user could get stuck in a
            // state where they cannot show it
            forceExpand(child)
            snapAnimator.cancel()
            false
        } else {
            false
        }
    }

    override fun onStopNestedScroll(
        coordinatorLayout: CoordinatorLayout,
        child: V,
        target: View,
        type: Int,
    ) {
        if (shouldSnapAfterScroll || type == ViewCompat.TYPE_NON_TOUCH) {
            if (expanded) {
                if (child.translationY >= anchorHeight / 2) {
                    animateSnap(child, SnapDirection.DOWN)
                } else {
                    animateSnap(child, SnapDirection.UP)
                }
            } else {
                if (child.translationY < (anchorHeight + child.height.toFloat() / 2)) {
                    animateSnap(child, SnapDirection.UP)
                } else {
                    animateSnap(child, SnapDirection.DOWN)
                }
            }
        }
    }

    override fun onNestedPreScroll(
        coordinatorLayout: CoordinatorLayout,
        child: V,
        target: View,
        dx: Int,
        dy: Int,
        consumed: IntArray,
        type: Int,
    ) {
        if (shouldScroll) {
            super.onNestedPreScroll(coordinatorLayout, child, target, dx, dy, consumed, type)
            child.translationY = max(
                0f,
                min(
                    child.height.toFloat() + anchorHeight,
                    child.translationY + dy,
                ),
            )
        }
    }

    override fun layoutDependsOn(
        parent: CoordinatorLayout,
        child: V,
        dependency: View,
    ): Boolean {
        engineView = parent.findViewInHierarchy { it is EngineView } as? EngineView
        val newAnchor = findAnchorInParent(parent)
        // The same valid anchor can report height 0 or the actual measured height
        // so checking for anchor equality is not enough, we need to check for height differences.
        if (anchorHeight != newAnchor?.height) {
            anchor = newAnchor
            dynamicDownload.translationY = -anchorHeight.toFloat()
        }
        return super.layoutDependsOn(parent, child, dependency)
    }

    fun forceExpand(view: View) {
        anchor = findAnchorInParent(view.parent as ViewGroup)
        animateSnap(view, SnapDirection.UP)
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun animateSnap(child: View, direction: SnapDirection) = with(snapAnimator) {
        expanded = direction == SnapDirection.UP
        addUpdateListener { child.translationY = it.animatedValue as Float }
        addListener(
            object : AnimatorListenerAdapter() {
                override fun onAnimationEnd(animation: Animator) {
                    // Ensure the right translationY if the anchor changes during the animation
                    dynamicDownload.translationY = -anchorHeight.toFloat()
                }
            },
        )
        setFloatValues(
            child.translationY,
            if (direction == SnapDirection.UP) {
                -anchorHeight.toFloat()
            } else {
                child.height.toFloat() + anchorHeight
            },
        )
        start()
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal enum class SnapDirection {
        UP,
        DOWN,
    }

    private fun findAnchorInParent(root: ViewGroup) =
        possibleAnchors
            .intersect(root.children.filter { it.isVisible && it.height > 0 }.map { it.id }.toSet()).firstOrNull()
            ?.let { root.findViewById<View>(it) }
}
