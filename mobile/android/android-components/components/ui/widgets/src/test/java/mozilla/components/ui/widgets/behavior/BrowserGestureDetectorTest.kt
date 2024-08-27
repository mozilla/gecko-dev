/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ui.widgets.behavior

import android.view.GestureDetector
import android.view.MotionEvent
import android.view.MotionEvent.ACTION_CANCEL
import android.view.MotionEvent.ACTION_DOWN
import android.view.MotionEvent.ACTION_MOVE
import android.view.MotionEvent.ACTION_POINTER_DOWN
import android.view.MotionEvent.ACTION_UP
import android.view.ScaleGestureDetector
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`

@RunWith(AndroidJUnit4::class)
class BrowserGestureDetectorTest {

    @Test
    fun `Detector should not attempt to detect zoom if MotionEvent's action is ACTION_CANCEL`() {
        val detector = BrowserGestureDetector(testContext, mock())
        val scaleDetector: ScaleGestureDetector = mock()
        detector.scaleGestureDetector = scaleDetector

        val downEvent = TestUtils.getMotionEvent(ACTION_DOWN)
        val cancelEvent = TestUtils.getMotionEvent(ACTION_CANCEL, previousEvent = downEvent)
        val moveEvent = TestUtils.getMotionEvent(ACTION_MOVE, previousEvent = downEvent)
        detector.handleTouchEvent(downEvent)
        detector.handleTouchEvent(cancelEvent)
        detector.handleTouchEvent(downEvent)
        detector.handleTouchEvent(moveEvent)

        verify(scaleDetector, times(3)).onTouchEvent(any<MotionEvent>())
    }

    @Test
    fun `Detector should not attempt to detect scrolls if a zoom gesture is in progress`() {
        val detector = BrowserGestureDetector(testContext, mock())
        val scrollDetector: GestureDetector = mock()
        val scaleDetector: ScaleGestureDetector = mock()
        detector.gestureDetector = scrollDetector
        detector.scaleGestureDetector = scaleDetector
        `when`(scaleDetector.isInProgress).thenReturn(true)
        val downEvent = TestUtils.getMotionEvent(ACTION_DOWN)
        val moveEvent = TestUtils.getMotionEvent(ACTION_MOVE, previousEvent = downEvent)

        detector.handleTouchEvent(downEvent)
        detector.handleTouchEvent(moveEvent)

        // In this case we let ACTION_DOWN, ACTION_UP, ACTION_CANCEL be handled but not others.
        verify(scrollDetector, times(1)).onTouchEvent(downEvent)
        verify(scrollDetector, never()).onTouchEvent(moveEvent)
    }

    @Test
    fun `Detector's handleTouchEvent returns false if the event was not handled`() {
        val detector = BrowserGestureDetector(testContext, mock())
        val unhandledEvent = TestUtils.getMotionEvent(ACTION_DOWN)

        // Neither the scale detector, nor the scroll detector should be interested
        // in a one of a time ACTION_CANCEL MotionEvent
        val wasEventHandled = detector.handleTouchEvent(
            TestUtils.getMotionEvent(ACTION_CANCEL, previousEvent = unhandledEvent),
        )

        assertFalse(wasEventHandled)
    }

    @Test
    fun `Detector's handleTouchEvent returns true if the event was handled`() {
        val detector = BrowserGestureDetector(testContext, mock())
        val downEvent = TestUtils.getMotionEvent(ACTION_DOWN)
        val moveEvent = TestUtils.getMotionEvent(ACTION_MOVE, 0f, 0f, previousEvent = downEvent)
        val moveEvent2 = TestUtils.getMotionEvent(ACTION_MOVE, 100f, 100f, previousEvent = moveEvent)

        detector.handleTouchEvent(downEvent)
        detector.handleTouchEvent(moveEvent)
        val wasEventHandled = detector.handleTouchEvent(moveEvent2)

        assertTrue(wasEventHandled)
    }

    @Test
    fun `Detector should inform about scroll and vertical scrolls events`() {
        var scrollValues = Pair(0f, 0f)
        var verticalScroll = 0f
        var horizontalScroll = 0f
        var scaleBeginCalled = false
        var scaleInProgressCalled = false
        var scaleEndCalled = false

        val gesturesListener = BrowserGestureDetector.GesturesListener(
            onScroll = { x, y -> scrollValues = Pair(x, y) },
            onVerticalScroll = { y -> verticalScroll = y },
            onHorizontalScroll = { x -> horizontalScroll = x },
            onScaleBegin = { scaleBeginCalled = true },
            onScale = { scaleInProgressCalled = true },
            onScaleEnd = { scaleEndCalled = true },
        )

        val detector = BrowserGestureDetector(testContext, gesturesListener)
        val downEvent = TestUtils.getMotionEvent(ACTION_DOWN)
        val moveEvent = TestUtils.getMotionEvent(ACTION_MOVE, 0f, 0f, previousEvent = downEvent)
        val moveEvent2 = TestUtils.getMotionEvent(ACTION_MOVE, 100f, 200f, previousEvent = moveEvent)

        detector.handleTouchEvent(downEvent)
        detector.handleTouchEvent(moveEvent)
        detector.handleTouchEvent(moveEvent2)

        // If the movement was more on the Y axis both "onScroll" and "onVerticalScroll" callbacks
        // should be called but no others.
        assertEquals(-200f, verticalScroll)
        assertEquals(0f, horizontalScroll)
        assertEquals(Pair(-100f, -200f), scrollValues)

        assertFalse(scaleBeginCalled)
        assertFalse(scaleInProgressCalled)
        assertFalse(scaleEndCalled)
    }

    @Test
    fun `Detector should prioritize vertical scrolls over horizontal scrolls`() {
        var scrollValues = Pair(0f, 0f)
        var verticalScroll = 0f
        var horizontalScroll = 0f
        var scaleBeginCalled = false
        var scaleInProgressCalled = false
        var scaleEndCalled = false

        val gesturesListener = BrowserGestureDetector.GesturesListener(
            onScroll = { x, y -> scrollValues = Pair(x, y) },
            onVerticalScroll = { y -> verticalScroll = y },
            onHorizontalScroll = { x -> horizontalScroll = x },
            onScaleBegin = { scaleBeginCalled = true },
            onScale = { scaleInProgressCalled = true },
            onScaleEnd = { scaleEndCalled = true },
        )

        val detector = BrowserGestureDetector(testContext, gesturesListener)
        val downEvent = TestUtils.getMotionEvent(ACTION_DOWN)
        val moveEvent = TestUtils.getMotionEvent(
            ACTION_MOVE,
            0f,
            0f,
            previousEvent = downEvent,
        )
        val moveEvent2 = TestUtils.getMotionEvent(
            ACTION_MOVE,
            100f,
            100f,
            previousEvent = moveEvent,
        )

        detector.handleTouchEvent(downEvent)
        detector.handleTouchEvent(moveEvent)
        detector.handleTouchEvent(moveEvent2)

        // If the movement was for the same amount on both the Y axis and the X axis
        // both "onScroll" and "onVerticalScroll" callbacks should be called but no others.
        assertEquals(-100f, verticalScroll)
        assertEquals(0f, horizontalScroll)
        assertEquals(Pair(-100f, -100f), scrollValues)

        assertFalse(scaleBeginCalled)
        assertFalse(scaleInProgressCalled)
        assertFalse(scaleEndCalled)
    }

    @Test
    fun `Detector should inform about scroll and horizontal scrolls events`() {
        var scrollValues = Pair(0f, 0f)
        var verticalScroll = 0f
        var horizontalScroll = 0f
        var scaleBeginCalled = false
        var scaleInProgressCalled = false
        var scaleEndCalled = false

        val gesturesListener = BrowserGestureDetector.GesturesListener(
            onScroll = { x, y -> scrollValues = Pair(x, y) },
            onVerticalScroll = { y -> verticalScroll = y },
            onHorizontalScroll = { x -> horizontalScroll = x },
            onScaleBegin = { scaleBeginCalled = true },
            onScale = { scaleInProgressCalled = true },
            onScaleEnd = { scaleEndCalled = true },
        )

        val detector = BrowserGestureDetector(testContext, gesturesListener)
        val downEvent = TestUtils.getMotionEvent(ACTION_DOWN)
        val moveEvent = TestUtils.getMotionEvent(ACTION_MOVE, 0f, 0f, previousEvent = downEvent)
        val moveEvent2 = TestUtils.getMotionEvent(ACTION_MOVE, 101f, 100f, previousEvent = moveEvent)

        detector.handleTouchEvent(downEvent)
        detector.handleTouchEvent(moveEvent)
        detector.handleTouchEvent(moveEvent2)

        // If the movement was for the same amount on both the Y axis and the X axis
        // both "onScroll" and "onVerticalScroll" callbacks should be called but no others.
        assertEquals(-101f, horizontalScroll)
        assertEquals(Pair(-101f, -100f), scrollValues)

        assertEquals(0f, verticalScroll)
        assertFalse(scaleBeginCalled)
        assertFalse(scaleInProgressCalled)
        assertFalse(scaleEndCalled)
    }

    @Test
    fun `Detector should always pass the ACTION_DOWN, ACTION_UP, ACTION_CANCEL events to the scroll detector`() {
        val detector = BrowserGestureDetector(testContext, mock())
        val scrollDetector: GestureDetector = mock()
        val scaleDetector: ScaleGestureDetector = mock()
        detector.gestureDetector = scrollDetector
        detector.scaleGestureDetector = scaleDetector
        // The aforementioned events should always be passed to the scroll detector,
        // even if scaling is in progress.
        `when`(scaleDetector.isInProgress).thenReturn(true)
        val downEvent = TestUtils.getMotionEvent(ACTION_DOWN)
        val moveEvent = TestUtils.getMotionEvent(ACTION_MOVE, previousEvent = downEvent)
        val upEvent = TestUtils.getMotionEvent(ACTION_UP, previousEvent = moveEvent)
        val cancelEvent = TestUtils.getMotionEvent(ACTION_CANCEL, previousEvent = upEvent)

        listOf(downEvent, moveEvent, upEvent, cancelEvent).forEach {
            detector.handleTouchEvent(it)
        }

        // With scaling in progress we let ACTION_DOWN, ACTION_UP, ACTION_CANCEL be handled but not others.
        verify(scrollDetector, times(1)).onTouchEvent(downEvent)
        verify(scrollDetector, times(1)).onTouchEvent(upEvent)
        verify(scrollDetector, times(1)).onTouchEvent(cancelEvent)
        verify(scrollDetector, never()).onTouchEvent(moveEvent)
    }

    @Test
    fun `Detector should not crash when the scroll detector receives a null first MotionEvent`() {
        var scrollValues = Pair(0f, 0f)
        var verticalScroll = 0f
        var horizontalScroll = 0f
        var scaleBeginCalled = false
        var scaleInProgressCalled = false
        var scaleEndCalled = false

        val gesturesListener = BrowserGestureDetector.GesturesListener(
            onScroll = { x, y -> scrollValues = Pair(x, y) },
            onVerticalScroll = { y -> verticalScroll = y },
            onHorizontalScroll = { x -> horizontalScroll = x },
            onScaleBegin = { scaleBeginCalled = true },
            onScale = { scaleInProgressCalled = true },
            onScaleEnd = { scaleEndCalled = true },
        )

        val crashReporting: CrashReporting = mock()
        val detector = BrowserGestureDetector(testContext, gesturesListener, crashReporting)
        // We need a previous event for ACTION_MOVE.
        // Don't use ACTION_DOWN (first pointer on the screen) but ACTION_POINTER_DOWN (other later pointer)
        // just to artificially be able to recreate the bug from 8552. This should not happen IRL.
        val downEvent = TestUtils.getMotionEvent(ACTION_POINTER_DOWN)
        val moveEvent = TestUtils.getMotionEvent(ACTION_MOVE, 0f, 0f, previousEvent = downEvent)
        val moveEvent2 = TestUtils.getMotionEvent(ACTION_MOVE, 100f, 200f, previousEvent = moveEvent)

        detector.handleTouchEvent(downEvent)
        detector.handleTouchEvent(moveEvent)
        detector.handleTouchEvent(moveEvent2)

        assertEquals(Pair(-100f, -200f), scrollValues)
        // We don't crash but neither can identify vertical / horizontal scrolls.

        assertEquals(0f, verticalScroll)
        assertEquals(0f, horizontalScroll)
        assertFalse(scaleBeginCalled)
        assertFalse(scaleInProgressCalled)
        assertFalse(scaleEndCalled)
    }
}
