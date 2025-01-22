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
 * Custom [CoordinatorLayout.Behavior] for ensuring autofill select bars are shown on top of the bottom toolbar.
 */
class AutofillSelectBarBehavior<V : View>(
    context: Context,
    toolbarPosition: ToolbarPosition,
) : CoordinatorLayout.Behavior<V>(context, null) {

    // Priority list of possible anchors for the autofill bars.
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
        // We have to check here if a new anchor is available and reparent the logins bar.
        if (anchorId != currentAnchorId) {
            positionAutofillBar(child, parent.children.firstOrNull { it.id == anchorId })
        }

        return false
    }

    private fun positionAutofillBar(autofillBar: V, dependency: View?) {
        currentAnchorId = dependency?.id ?: View.NO_ID

        autofillBar.post {
            when (dependency == null) {
                true -> placeAtBottom(autofillBar)
                false -> placeAboveAnchor(autofillBar, dependency)
            }
        }
    }

    /**
     * Place the autofill bar at the bottom of the screen.
     */
    fun placeAtBottom(autofillBar: View) {
        val params = autofillBar.layoutParams as CoordinatorLayout.LayoutParams

        params.anchorId = View.NO_ID
        params.anchorGravity = Gravity.NO_GRAVITY
        params.gravity = Gravity.BOTTOM or Gravity.CENTER_HORIZONTAL
        autofillBar.layoutParams = params
    }

    /**
     * Place the autofill bar above the given anchor.
     */
    fun placeAboveAnchor(autofillBar: View, anchor: View) {
        val params = autofillBar.layoutParams as CoordinatorLayout.LayoutParams

        params.anchorId = anchor.id
        params.anchorGravity = Gravity.TOP or Gravity.CENTER_HORIZONTAL
        params.gravity = Gravity.TOP or Gravity.CENTER_HORIZONTAL
        autofillBar.layoutParams = params
    }
}
