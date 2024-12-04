/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.coordinatorlayout.widget.CoordinatorLayout
import io.mockk.every
import io.mockk.mockk
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class LoginSelectBarBehaviorTest {
    private val loginsBar = mockk<FrameLayout>(relaxed = true)
    private var layoutParams = CoordinatorLayout.LayoutParams(0, 0)
    private val dependency = View(testContext)
    private val parent = CoordinatorLayout(testContext)

    @Before
    fun setup() {
        every { loginsBar.layoutParams } returns layoutParams
        every { loginsBar.post(any()) } answers {
            // Immediately run the given Runnable argument
            val action: Runnable = firstArg()
            action.run()
            true
        }
        parent.addView(dependency)
    }

    @Test
    fun `GIVEN no valid anchors are shown WHEN the login bar is shown THEN don't anchor it`() {
        val behavior = LoginSelectBarBehavior<ViewGroup>(testContext, ToolbarPosition.BOTTOM)

        behavior.layoutDependsOn(parent, loginsBar, dependency)

        assertLoginsBarIsPlacedAtTheBottomOfTheScreen()
    }

    @Test
    fun `GIVEN only a toolbar shown at top WHEN the login bar is shown THEN don't anchor it`() {
        dependency.id = R.id.toolbar
        val behavior = LoginSelectBarBehavior<ViewGroup>(testContext, ToolbarPosition.TOP)

        behavior.layoutDependsOn(parent, loginsBar, dependency)

        assertLoginsBarIsPlacedAtTheBottomOfTheScreen()
    }

    @Test
    fun `GIVEN a toolbar shown at top and the navbar at the bottom WHEN the login bar is shown THEN anchor it to the navbar`() {
        dependency.id = R.id.toolbar_navbar_container
        val behavior = LoginSelectBarBehavior<ViewGroup>(testContext, ToolbarPosition.TOP)

        behavior.layoutDependsOn(parent, loginsBar, dependency)

        assertLoginsBarPlacementAboveAnchor()
    }

    @Test
    fun `GIVEN only a toolbar shown at bottom WHEN the login bar is shown THEN anchor it to the toolbar`() {
        dependency.id = R.id.toolbar
        val behavior = LoginSelectBarBehavior<ViewGroup>(testContext, ToolbarPosition.BOTTOM)

        behavior.layoutDependsOn(parent, loginsBar, dependency)

        assertLoginsBarPlacementAboveAnchor()
    }

    @Test
    fun `GIVEN the login bar is anchored to the bottom toolbar WHEN the toolbar is not shown anymore THEN place the login bar at the bottom`() {
        val toolbar = View(testContext)
            .apply { id = R.id.toolbar }
            .also { parent.addView(it) }
        val behavior = LoginSelectBarBehavior<ViewGroup>(testContext, ToolbarPosition.BOTTOM)

        // Test the scenario where the toolbar is invisible.
        behavior.layoutDependsOn(parent, loginsBar, dependency)
        assertLoginsBarPlacementAboveAnchor(toolbar)
        toolbar.visibility = View.GONE
        behavior.layoutDependsOn(parent, loginsBar, dependency)
        assertLoginsBarIsPlacedAtTheBottomOfTheScreen()

        // Test the scenario where the toolbar is removed from parent.
        toolbar.visibility = View.VISIBLE
        behavior.layoutDependsOn(parent, loginsBar, dependency)
        assertLoginsBarPlacementAboveAnchor(toolbar)
        parent.removeView(toolbar)
        behavior.layoutDependsOn(parent, loginsBar, dependency)
        assertLoginsBarIsPlacedAtTheBottomOfTheScreen()
    }

    @Test
    fun `GIVEN the login bar is anchored based on a top toolbar WHEN the toolbar is not shown anymore THEN place the login bar at the bottom`() {
        val toolbar = View(testContext)
            .apply { id = R.id.toolbar }
            .also { parent.addView(it) }
        val behavior = LoginSelectBarBehavior<ViewGroup>(testContext, ToolbarPosition.TOP)

        // Test the scenario where the toolbar is invisible.
        behavior.layoutDependsOn(parent, loginsBar, dependency)
        assertLoginsBarIsPlacedAtTheBottomOfTheScreen()
        toolbar.visibility = View.GONE
        behavior.layoutDependsOn(parent, loginsBar, dependency)
        assertLoginsBarIsPlacedAtTheBottomOfTheScreen()

        // Test the scenario where the toolbar is removed from parent.
        toolbar.visibility = View.VISIBLE
        behavior.layoutDependsOn(parent, loginsBar, dependency)
        assertLoginsBarIsPlacedAtTheBottomOfTheScreen()
        parent.removeView(toolbar)
        behavior.layoutDependsOn(parent, loginsBar, dependency)
        assertLoginsBarIsPlacedAtTheBottomOfTheScreen()
    }

    @Test
    fun `GIVEN the login bar is anchored to the navbar WHEN the navbar is not shown anymore THEN place the login bar at the bottom`() {
        val navbar = View(testContext)
            .apply { id = R.id.toolbar_navbar_container }
            .also { parent.addView(it) }
        val behavior = LoginSelectBarBehavior<ViewGroup>(testContext, ToolbarPosition.BOTTOM)

        // Test the scenario where the toolbar is invisible.
        behavior.layoutDependsOn(parent, loginsBar, dependency)
        assertLoginsBarPlacementAboveAnchor(navbar)
        navbar.visibility = View.GONE
        behavior.layoutDependsOn(parent, loginsBar, dependency)
        assertLoginsBarIsPlacedAtTheBottomOfTheScreen()

        // Test the scenario where the toolbar is removed from parent.
        navbar.visibility = View.VISIBLE
        behavior.layoutDependsOn(parent, loginsBar, dependency)
        assertLoginsBarPlacementAboveAnchor(navbar)
        parent.removeView(navbar)
        behavior.layoutDependsOn(parent, loginsBar, dependency)
        assertLoginsBarIsPlacedAtTheBottomOfTheScreen()
    }

    @Test
    fun `GIVEN a login bar WHEN asked to place it at bottom THEN anchor it to the bottom of the screen`() {
        val behavior = LoginSelectBarBehavior<ViewGroup>(testContext, ToolbarPosition.BOTTOM)

        behavior.placeAtBottom(loginsBar)

        assertLoginsBarIsPlacedAtTheBottomOfTheScreen()
    }

    @Test
    fun `GIVEN a login bar WHEN asked to place it above a certain anchor THEN anchor it to the indicated view`() {
        val behavior = LoginSelectBarBehavior<ViewGroup>(testContext, ToolbarPosition.BOTTOM)
        dependency.id = R.id.toolbar

        behavior.placeAboveAnchor(loginsBar, dependency)

        assertLoginsBarPlacementAboveAnchor()
    }

    private fun assertLoginsBarPlacementAboveAnchor(anchor: View = dependency) {
        assertEquals(anchor.id, layoutParams.anchorId)
        assertEquals(Gravity.TOP or Gravity.CENTER_HORIZONTAL, layoutParams.anchorGravity)
        assertEquals(Gravity.TOP or Gravity.CENTER_HORIZONTAL, layoutParams.gravity)
    }

    private fun assertLoginsBarIsPlacedAtTheBottomOfTheScreen() {
        assertEquals(View.NO_ID, layoutParams.anchorId)
        assertEquals(Gravity.NO_GRAVITY, layoutParams.anchorGravity)
        assertEquals(Gravity.BOTTOM or Gravity.CENTER_HORIZONTAL, layoutParams.gravity)
    }
}
