/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import android.util.AttributeSet
import android.view.Gravity
import android.view.ViewGroup
import android.widget.LinearLayout
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.ComposeView
import androidx.coordinatorlayout.widget.CoordinatorLayout
import mozilla.components.concept.toolbar.ScrollableToolbar
import mozilla.components.ui.widgets.behavior.EngineViewScrollingBehavior
import mozilla.components.ui.widgets.behavior.ViewPosition
import org.mozilla.fenix.R

/**
 *  A helper class to add a bottom toolbar container view to the given [parent].
 *
 * @param context The [Context] the view is running in.
 * @param parent The [ViewGroup] into which the composable will be added.
 * @param hideOnScroll If the container should react to the EngineView content being scrolled.
 * @param content The content of the toolbar to display.
 */
class BottomToolbarContainerView(
    private val context: Context,
    private val parent: ViewGroup,
    private val hideOnScroll: Boolean = false,
    private val content: @Composable () -> Unit,
) {

    val toolbarContainerView = ToolbarContainerView(context).apply {
        id = R.id.toolbar_navbar_container
    }
    private val composeView = ComposeView(context).apply {
        setContent {
            content()
        }
        toolbarContainerView.addView(this)
        toolbarContainerView.isClickable = true
    }

    init {
        toolbarContainerView.layoutParams = CoordinatorLayout.LayoutParams(
            CoordinatorLayout.LayoutParams.MATCH_PARENT,
            CoordinatorLayout.LayoutParams.WRAP_CONTENT,
        ).apply {
            gravity = Gravity.BOTTOM
            if (hideOnScroll) {
                behavior = EngineViewScrollingBehavior(parent.context, null, ViewPosition.BOTTOM)
            }
        }

        parent.addView(toolbarContainerView)
    }

    /**
     * Updates the Composable content of the [composeView].
     */
    fun updateContent(content: @Composable () -> Unit) {
        composeView.setContent {
            content()
        }
    }
}

/**
 * A container view that hosts a navigation bar and, possibly, a toolbar.
 * Facilitates hide-on-scroll behavior.
 */
class ToolbarContainerView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
) : LinearLayout(context, attrs, defStyleAttr), ScrollableToolbar {
    override fun enableScrolling() {
        (layoutParams as? CoordinatorLayout.LayoutParams)?.apply {
            (behavior as? EngineViewScrollingBehavior)?.enableScrolling()
        }
    }

    override fun disableScrolling() {
        (layoutParams as? CoordinatorLayout.LayoutParams)?.apply {
            (behavior as? EngineViewScrollingBehavior)?.disableScrolling()
        }
    }

    override fun expand() {
        (layoutParams as? CoordinatorLayout.LayoutParams)?.apply {
            (behavior as? EngineViewScrollingBehavior)?.forceExpand(this@ToolbarContainerView)
        }
    }

    override fun collapse() {
        (layoutParams as? CoordinatorLayout.LayoutParams)?.apply {
            (behavior as? EngineViewScrollingBehavior)?.forceCollapse(this@ToolbarContainerView)
        }
    }
}
