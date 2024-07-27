/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import android.annotation.SuppressLint
import android.content.Context
import android.view.MotionEvent
import androidx.annotation.VisibleForTesting
import androidx.core.view.NestedScrollingChild
import androidx.core.view.NestedScrollingChildHelper
import androidx.core.view.ViewCompat
import mozilla.components.concept.engine.InputResultDetail
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoView
import org.mozilla.geckoview.PanZoomController

/**
 * geckoView that supports nested scrolls (for using in a CoordinatorLayout).
 *
 * This code is a simplified version of the NestedScrollView implementation
 * which can be found in the support library:
 * [android.support.v4.widget.NestedScrollView]
 *
 * Based on:
 * https://github.com/takahirom/webview-in-coordinatorlayout
 */

@Suppress("ClickableViewAccessibility")
open class NestedGeckoView(context: Context) : GeckoView(context), NestedScrollingChild {
    @VisibleForTesting
    internal var lastY: Int = 0

    @VisibleForTesting
    internal val scrollOffset = IntArray(2)

    private val scrollConsumed = IntArray(2)

    private var gestureCanReachParent = true

    /**
     * Represents the initial scroll.
     */
    enum class InitialScrollDirection {
        NOT_YET,
        DOWN,
        UP,
    }

    private var initialScrollDirection = InitialScrollDirection.NOT_YET

    private var initialDownY: Float = 0f

    @VisibleForTesting
    internal var nestedOffsetY: Int = 0

    @VisibleForTesting
    internal var childHelper: NestedScrollingChildHelper = NestedScrollingChildHelper(this)

    /**
     * How user's MotionEvent will be handled.
     *
     * @see InputResultDetail
     */
    internal var inputResultDetail = InputResultDetail.newInstance(true)

    init {
        isNestedScrollingEnabled = true
    }

    @Suppress("ComplexMethod")
    override fun onTouchEvent(ev: MotionEvent): Boolean {
        val event = MotionEvent.obtain(ev)
        val action = ev.actionMasked
        val eventY = event.y.toInt()

        when (action) {
            MotionEvent.ACTION_MOVE -> {
                val allowScroll = !shouldPinOnScreen() && inputResultDetail.isTouchHandledByBrowser()

                var deltaY = lastY - eventY

                if (allowScroll && dispatchNestedPreScroll(0, deltaY, scrollConsumed, scrollOffset)) {
                    deltaY -= scrollConsumed[1]
                    event.offsetLocation(0f, (-scrollOffset[1]).toFloat())
                    nestedOffsetY += scrollOffset[1]
                }

                lastY = eventY - scrollOffset[1]

                if (allowScroll && dispatchNestedScroll(0, scrollOffset[1], 0, deltaY, scrollOffset)) {
                    lastY -= scrollOffset[1]
                    event.offsetLocation(0f, scrollOffset[1].toFloat())
                    nestedOffsetY += scrollOffset[1]
                }

                if (initialScrollDirection == InitialScrollDirection.NOT_YET) {
                    if (event.y > initialDownY) {
                        initialScrollDirection = InitialScrollDirection.UP
                    } else if (event.y < initialDownY) {
                        initialScrollDirection = InitialScrollDirection.DOWN
                    } else {
                        // We may want to disallow pull-to-refresh in the case of
                        // scrolling left or right initially?
                    }
                }
            }

            MotionEvent.ACTION_DOWN -> {
                // A new gesture started. Ask GV if it can handle this.
                parent?.requestDisallowInterceptTouchEvent(true)
                updateInputResult(event)

                initialScrollDirection = InitialScrollDirection.NOT_YET
                nestedOffsetY = 0
                lastY = eventY
                initialDownY = event.y

                // The event should be handled either by onTouchEvent,
                // either by onTouchEventForResult, never by both.
                // Early return if we sent it to updateInputResult(..) which calls onTouchEventForResult.
                event.recycle()
                return true
            }

            // We don't care about other touch events
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                // inputResultDetail needs to be reset here and not in the next ACTION_DOWN, because
                // its value is used by other features that poll for the value via
                // `EngineView.getInputResultDetail`. Not resetting this in ACTION_CANCEL/ACTION_UP
                // would then mean we send stale information to those features from a previous
                // gesture's result.
                inputResultDetail = InputResultDetail.newInstance(true)
                stopNestedScroll()

                // Allow touch event interception here so that the next ACTION_DOWN event can be properly
                // intercepted by the parent.
                parent?.requestDisallowInterceptTouchEvent(false)
                gestureCanReachParent = true
            }
        }

        // Execute event handler from parent class in all cases
        val eventHandled = callSuperOnTouchEvent(event)

        // Recycle previously obtained event
        event.recycle()

        return eventHandled
    }

    @VisibleForTesting
    internal fun callSuperOnTouchEvent(event: MotionEvent): Boolean {
        return super.onTouchEvent(event)
    }

    @SuppressLint("WrongThread") // Lint complains startNestedScroll() needs to be called on the main thread
    @VisibleForTesting
    internal fun updateInputResult(event: MotionEvent) {
        val eventAction = event.actionMasked
        superOnTouchEventForDetailResult(event)
            .accept {
                // Since the response from APZ is async, we could theoretically have a response
                // which is out of time when we get the ACTION_MOVE events, and we do not want
                // to forward this to the parent pre-emptively.
                if (!gestureCanReachParent) {
                    return@accept
                }

                inputResultDetail = inputResultDetail.copy(
                    it?.handledResult(),
                    it?.scrollableDirections(),
                    it?.overscrollDirections(),
                )

                if (eventAction == MotionEvent.ACTION_DOWN) {
                    // Gesture can reach the parent only if the content is already at the top
                    gestureCanReachParent = inputResultDetail.canOverscrollTop()

                    when (initialScrollDirection) {
                        InitialScrollDirection.NOT_YET -> {
                            // If the event wasn't used in GeckoView, allow touch event interceptiona.
                            if (gestureCanReachParent && inputResultDetail.isTouchUnhandled()) {
                                parent?.requestDisallowInterceptTouchEvent(false)
                            }
                        }

                        InitialScrollDirection.UP -> {
                            // If we received the InputResultDetail from Gecko after we've sent the
                            // first touch move event to Gecko, that means there's at least a touch
                            // event listener whether to prevent the event or not, or CSS touch-action
                            // properties in the contents, thus if the result isn't
                            // `INPUT_HANDLED_CONTENT`, we can allow pull-to-refresh.
                            if (gestureCanReachParent && !inputResultDetail.isTouchHandledByWebsite()) {
                                parent?.requestDisallowInterceptTouchEvent(false)
                            }
                        }

                        InitialScrollDirection.DOWN -> {
                            parent?.requestDisallowInterceptTouchEvent(true)
                        }
                    }
                }

                startNestedScroll(ViewCompat.SCROLL_AXIS_VERTICAL)
            }
    }

    @VisibleForTesting
    internal open fun superOnTouchEventForDetailResult(
        event: MotionEvent,
    ): GeckoResult<PanZoomController.InputResultDetail> =
        super.onTouchEventForDetailResult(event)

    override fun setNestedScrollingEnabled(enabled: Boolean) {
        childHelper.isNestedScrollingEnabled = enabled
    }

    override fun isNestedScrollingEnabled(): Boolean {
        return childHelper.isNestedScrollingEnabled
    }

    override fun startNestedScroll(axes: Int): Boolean {
        return childHelper.startNestedScroll(axes)
    }

    override fun stopNestedScroll() {
        childHelper.stopNestedScroll()
    }

    override fun hasNestedScrollingParent(): Boolean {
        return childHelper.hasNestedScrollingParent()
    }

    override fun dispatchNestedScroll(
        dxConsumed: Int,
        dyConsumed: Int,
        dxUnconsumed: Int,
        dyUnconsumed: Int,
        offsetInWindow: IntArray?,
    ): Boolean {
        return childHelper.dispatchNestedScroll(dxConsumed, dyConsumed, dxUnconsumed, dyUnconsumed, offsetInWindow)
    }

    override fun dispatchNestedPreScroll(dx: Int, dy: Int, consumed: IntArray?, offsetInWindow: IntArray?): Boolean {
        return childHelper.dispatchNestedPreScroll(dx, dy, consumed, offsetInWindow)
    }

    override fun dispatchNestedFling(velocityX: Float, velocityY: Float, consumed: Boolean): Boolean {
        return childHelper.dispatchNestedFling(velocityX, velocityY, consumed)
    }

    override fun dispatchNestedPreFling(velocityX: Float, velocityY: Float): Boolean {
        return childHelper.dispatchNestedPreFling(velocityX, velocityY)
    }
}
