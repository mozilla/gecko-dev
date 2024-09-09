/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.dialog

import android.animation.ValueAnimator
import android.view.View
import android.view.ViewGroup
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.view.ViewCompat
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.InputResultDetail
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class DynamicDownloadDialogBehaviorTest {
    private val downloadDialog: View = mockk()
    private val settings: Settings = mockk()
    private val appStore: AppStore = mockk()
    private lateinit var behavior: DynamicDownloadDialogBehavior<View>

    @Before
    fun setup() {
        every { downloadDialog.context } returns testContext
        every { settings.toolbarPosition } returns ToolbarPosition.BOTTOM
        behavior = spyk(DynamicDownloadDialogBehavior(downloadDialog, settings, appStore))
    }

    @Test
    fun `Starting a nested scroll should cancel an ongoing snap animation`() {
        every { behavior.shouldScroll } returns true

        val animator: ValueAnimator = mockk(relaxed = true)
        behavior.snapAnimator = animator

        val acceptsNestedScroll = behavior.onStartNestedScroll(
            coordinatorLayout = mockk(),
            child = mockk(),
            directTargetChild = mockk(),
            target = mockk(),
            axes = ViewCompat.SCROLL_AXIS_VERTICAL,
            type = ViewCompat.TYPE_TOUCH,
        )

        assertTrue(acceptsNestedScroll)

        verify { animator.cancel() }
    }

    @Test
    fun `Behavior should not accept nested scrolls on the horizontal axis`() {
        val acceptsNestedScroll = behavior.onStartNestedScroll(
            coordinatorLayout = mockk(),
            child = mockk(),
            directTargetChild = mockk(),
            target = mockk(),
            axes = ViewCompat.SCROLL_AXIS_HORIZONTAL,
            type = ViewCompat.TYPE_TOUCH,
        )

        assertFalse(acceptsNestedScroll)
    }

    @Test
    fun `Behavior will snap the dialog up if it is more than 50 percent visible`() {
        every { behavior.shouldScroll } returns true
        behavior.anchor = mockk<View> {
            every { height } returns 10
        }

        val animator: ValueAnimator = mockk(relaxed = true)
        behavior.snapAnimator = animator

        behavior.expanded = false

        val child = mockk<View> {
            every { height } returns 100
            every { translationY } returns 59f
        }

        behavior.onStartNestedScroll(
            coordinatorLayout = mockk(),
            child = child,
            directTargetChild = mockk(),
            target = mockk(),
            axes = ViewCompat.SCROLL_AXIS_VERTICAL,
            type = ViewCompat.TYPE_TOUCH,
        )

        assertTrue(behavior.shouldSnapAfterScroll)

        verify(exactly = 0) { animator.start() }

        behavior.onStopNestedScroll(
            coordinatorLayout = mockk(),
            child = child,
            target = mockk(),
            type = 0,
        )

        verify { behavior.animateSnap(child, DynamicDownloadDialogBehavior.SnapDirection.UP) }

        verify { animator.start() }
    }

    @Test
    fun `Behavior will snap the dialog down if translationY is at least equal to half the toolbarHeight`() {
        every { behavior.shouldScroll } returns true
        behavior.anchor = mockk<View> {
            every { height } returns 10
        }

        val animator: ValueAnimator = mockk(relaxed = true)
        behavior.snapAnimator = animator

        behavior.expanded = true

        val child = mockk<View> {
            every { height } returns 100
            every { translationY } returns 5f
        }

        behavior.onStartNestedScroll(
            coordinatorLayout = mockk(),
            child = child,
            directTargetChild = mockk(),
            target = mockk(),
            axes = ViewCompat.SCROLL_AXIS_VERTICAL,
            type = ViewCompat.TYPE_TOUCH,
        )

        assertTrue(behavior.shouldSnapAfterScroll)

        verify(exactly = 0) { animator.start() }

        behavior.onStopNestedScroll(
            coordinatorLayout = mockk(),
            child = child,
            target = mockk(),
            type = 0,
        )

        verify { behavior.animateSnap(child, DynamicDownloadDialogBehavior.SnapDirection.DOWN) }

        verify { animator.start() }
    }

    @Test
    fun `Behavior will apply translation to the dialog for nested scroll`() {
        every { behavior.shouldScroll } returns true
        behavior.anchor = mockk<View> {
            every { height } returns 10
        }

        val child = mockk<View> {
            every { height } returns 100
            every { translationY } returns 0f
            every { translationY = any() } returns Unit
        }

        behavior.onNestedPreScroll(
            coordinatorLayout = mockk(),
            child = child,
            target = mockk(),
            dx = 0,
            dy = 25,
            consumed = IntArray(0),
            type = 0,
        )

        verify { child.translationY = 25f }
    }

    @Test
    fun `Behavior will animateSnap UP when forceExpand is called`() {
        val dynamicDialogView: View = mockk(relaxed = true) {
            every { parent } returns mockk<ViewGroup>(relaxed = true)
        }
        every { behavior.shouldScroll } returns true

        behavior.forceExpand(dynamicDialogView)

        verify {
            behavior.animateSnap(
                dynamicDialogView,
                DynamicDownloadDialogBehavior.SnapDirection.UP,
            )
        }
    }

    @Test
    fun `GIVEN an EngineView is not available WHEN shouldScroll is called THEN it returns false`() {
        behavior.engineView = null

        assertFalse(behavior.shouldScroll)
    }

    @Test
    fun `GIVEN an InputResultDetail with the right values WHEN shouldScroll is called THEN it returns true`() {
        val engineView: EngineView = mockk()
        behavior.engineView = engineView
        val validInputResultDetail: InputResultDetail = mockk()
        every { engineView.getInputResultDetail() } returns validInputResultDetail

        every { validInputResultDetail.canScrollToBottom() } returns true
        every { validInputResultDetail.canScrollToTop() } returns false
        assertTrue(behavior.shouldScroll)

        every { validInputResultDetail.canScrollToBottom() } returns false
        every { validInputResultDetail.canScrollToTop() } returns true
        assertTrue(behavior.shouldScroll)

        every { validInputResultDetail.canScrollToBottom() } returns true
        every { validInputResultDetail.canScrollToTop() } returns true
        assertTrue(behavior.shouldScroll)
    }

    @Test
    fun `GIVEN a gesture that doesn't scroll the toolbar WHEN startNestedScroll THEN toolbar is expanded and nested scroll not accepted`() {
        val engineView: EngineView = mockk()
        behavior.engineView = engineView
        val inputResultDetail: InputResultDetail = mockk()
        val animator: ValueAnimator = mockk(relaxed = true)
        behavior.snapAnimator = animator
        every { behavior.shouldScroll } returns false
        every { behavior.forceExpand(any()) } just Runs
        every { engineView.getInputResultDetail() } returns inputResultDetail
        every { inputResultDetail.isTouchUnhandled() } returns true

        val childView: View = mockk()
        val acceptsNestedScroll = behavior.onStartNestedScroll(
            coordinatorLayout = mockk(),
            child = childView,
            directTargetChild = mockk(),
            target = mockk(),
            axes = ViewCompat.SCROLL_AXIS_VERTICAL,
            type = ViewCompat.TYPE_TOUCH,
        )

        verify { behavior.forceExpand(childView) }
        verify { animator.cancel() }
        assertFalse(acceptsNestedScroll)
    }

    @Test
    fun `GIVEN toolbar at top WHEN the layout is updated THEN the anchor is correctly inferred`() {
        every { settings.toolbarPosition } returns ToolbarPosition.TOP
        behavior = DynamicDownloadDialogBehavior(downloadDialog, settings, appStore)
        val anchor = View(testContext).apply {
            id = R.id.toolbar_navbar_container
        }
        val rootLayout = CoordinatorLayout(testContext).apply {
            addView(View(testContext))
            addView(anchor)
            addView(View(testContext))
        }

        assertNull(behavior.anchor)

        val result = behavior.layoutDependsOn(rootLayout, mockk(), mockk())

        assertFalse(result)
        assertSame(anchor, behavior.anchor)
    }

    @Test
    fun `GIVEN toolbar at top WHEN the layout is updated THEN a bottom anchor might be missing`() {
        every { settings.toolbarPosition } returns ToolbarPosition.TOP
        behavior = DynamicDownloadDialogBehavior(downloadDialog, settings, appStore)
        val anchor = View(testContext).apply {
            id = R.id.toolbar
        }
        val rootLayout = CoordinatorLayout(testContext).apply {
            addView(View(testContext))
            addView(anchor)
            addView(View(testContext))
        }

        assertNull(behavior.anchor)

        val result = behavior.layoutDependsOn(rootLayout, mockk(), mockk())

        assertFalse(result)
        assertNull(behavior.anchor)
    }

    @Test
    fun `GIVEN toolbar at bottom WHEN the layout is updated THEN the anchor is correctly inferred`() {
        every { settings.toolbarPosition } returns ToolbarPosition.BOTTOM
        behavior = DynamicDownloadDialogBehavior(downloadDialog, settings, appStore)
        val anchor = View(testContext).apply {
            id = listOf(R.id.toolbar_navbar_container, R.id.toolbar).random()
        }
        val rootLayout = CoordinatorLayout(testContext).apply {
            addView(View(testContext))
            addView(anchor)
            addView(View(testContext))
        }

        assertNull(behavior.anchor)

        val result = behavior.layoutDependsOn(rootLayout, mockk(), mockk())

        assertFalse(result)
        assertSame(anchor, behavior.anchor)
    }

    @Test
    fun `GIVEN toolbar at top WHEN the dynamic download dialog is expanded THEN the anchor is correctly inferred`() {
        every { settings.toolbarPosition } returns ToolbarPosition.TOP
        behavior = DynamicDownloadDialogBehavior(downloadDialog, settings, appStore)
        val anchor = View(testContext).apply {
            id = R.id.toolbar_navbar_container
        }
        val rootLayout = CoordinatorLayout(testContext).apply {
            addView(View(testContext))
            addView(anchor)
            addView(View(testContext))
        }
        val dynamicDialogView: View = mockk(relaxed = true) {
            every { parent } returns rootLayout
        }

        assertNull(behavior.anchor)

        behavior.forceExpand(dynamicDialogView)

        assertSame(anchor, behavior.anchor)
    }

    @Test
    fun `GIVEN toolbar at top WHEN the dynamic download dialog is expanded THEN a bottom anchor might be missing`() {
        every { settings.toolbarPosition } returns ToolbarPosition.TOP
        behavior = DynamicDownloadDialogBehavior(downloadDialog, settings, appStore)
        val anchor = View(testContext).apply {
            id = R.id.toolbar
        }
        val rootLayout = CoordinatorLayout(testContext).apply {
            addView(View(testContext))
            addView(anchor)
            addView(View(testContext))
        }
        val dynamicDialogView: View = mockk(relaxed = true) {
            every { parent } returns rootLayout
        }

        assertNull(behavior.anchor)

        behavior.forceExpand(dynamicDialogView)

        assertNull(behavior.anchor)
    }

    @Test
    fun `GIVEN toolbar at bottom WHEN the dynamic download dialog is expanded THEN the anchor is correctly inferred`() {
        every { settings.toolbarPosition } returns ToolbarPosition.BOTTOM
        behavior = DynamicDownloadDialogBehavior(downloadDialog, settings, appStore)
        val anchor = View(testContext).apply {
            id = listOf(R.id.toolbar_navbar_container, R.id.toolbar).random()
        }
        val rootLayout = CoordinatorLayout(testContext).apply {
            addView(View(testContext))
            addView(anchor)
            addView(View(testContext))
        }
        val dynamicDialogView: View = mockk(relaxed = true) {
            every { parent } returns rootLayout
        }

        assertNull(behavior.anchor)

        behavior.forceExpand(dynamicDialogView)

        assertSame(anchor, behavior.anchor)
    }
}
