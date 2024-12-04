/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.content.Context
import android.view.Gravity
import android.view.View
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.view.children
import androidx.core.view.isVisible
import org.mozilla.fenix.R
import org.mozilla.fenix.components.toolbar.ToolbarPosition

/**
 * Custom [CoordinatorLayout.Behavior] for ensuring login bars are shown on top of the bottom toolbar.
 */
class LoginSelectBarBehavior<V : View>(
    context: Context,
    toolbarPosition: ToolbarPosition,
) : CoordinatorLayout.Behavior<V>(context, null) {

    // Priority list of possible anchors for the logins bars.
    private val dependenciesIds = buildList {
        add(R.id.toolbar_navbar_container)
        if (toolbarPosition == ToolbarPosition.BOTTOM) {
            add(R.id.toolbar)
        }
    }

    private var currentAnchorId: Int? = View.NO_ID

    override fun layoutDependsOn(
        parent: CoordinatorLayout,
        child: V,
        dependency: View,
    ): Boolean {
        val anchorId = dependenciesIds
            .intersect(parent.children.filter { it.isVisible }.map { it.id }.toSet())
            .firstOrNull()

        // It is possible that previous anchor's visibility is changed.
        // The layout is updated and layoutDependsOn is called but onDependentViewChanged not.
        // We have to check here if a new anchor is available and reparent the logins bar.
        // This check also ensures we are not positioning the login bar multiple times for the same anchor.
        return if (anchorId != currentAnchorId) {
            positionLoginBar(child, parent.children.firstOrNull { it.id == anchorId })
            true
        } else {
            false
        }
    }

    private fun positionLoginBar(loginBar: V, dependency: View?) {
        currentAnchorId = dependency?.id ?: View.NO_ID
        val params = loginBar.layoutParams as CoordinatorLayout.LayoutParams

        loginBar.post {
            if (dependency == null) {
                // Position the login bar at the bottom of the screen.
                params.anchorId = View.NO_ID
                params.anchorGravity = Gravity.NO_GRAVITY
                params.gravity = Gravity.BOTTOM or Gravity.CENTER_HORIZONTAL
                loginBar.layoutParams = params
            } else {
                // Position the login bar just above the anchor.
                params.anchorId = dependency.id
                params.anchorGravity = Gravity.TOP or Gravity.CENTER_HORIZONTAL
                params.gravity = Gravity.TOP or Gravity.CENTER_HORIZONTAL
                loginBar.layoutParams = params
            }
        }
    }
}
