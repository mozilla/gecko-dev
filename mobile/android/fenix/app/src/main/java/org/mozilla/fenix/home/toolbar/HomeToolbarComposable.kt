/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.toolbar

import android.content.Context
import android.view.Gravity
import android.view.ViewGroup
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ComposeView
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.view.updateLayoutParams
import mozilla.components.browser.state.helper.Target
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.compose.base.Divider
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.compose.browser.toolbar.BrowserToolbar
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarStore
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.tabstrip.isTabStripEnabled
import org.mozilla.fenix.components.toolbar.ToolbarPosition.BOTTOM
import org.mozilla.fenix.components.toolbar.ToolbarPosition.TOP
import org.mozilla.fenix.databinding.FragmentHomeBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.utils.Settings

/**
 * A wrapper over the [BrowserToolbar] composable to allow for extra customisation and
 * integration in the same framework as the [HomeToolbarView].
 *
 * @param context [Context] used for various system interactions.
 * @param homeBinding [FragmentHomeBinding] which will serve as parent for this composable.
 * @param settings [Settings] for querying various application settings.
 * @param tabStripContent [Composable] as the tab strip content to be displayed together with this toolbar.
 */
internal class HomeToolbarComposable(
    private val context: Context,
    private val homeBinding: FragmentHomeBinding,
    private val settings: Settings,
    private val tabStripContent: @Composable () -> Unit,
) : FenixHomeToolbar {
    private var showDivider by mutableStateOf(true)

    override val layout = ComposeView(context).apply {
        setContent {
            val shouldShowDivider by remember { mutableStateOf(showDivider) }
            val shouldShowTabStrip: Boolean = remember { context.isTabStripEnabled() }

            AcornTheme {
                when (shouldShowTabStrip) {
                    true -> Column {
                        tabStripContent()
                        BrowserToolbar(shouldShowDivider, settings.shouldUseBottomToolbar)
                    }

                    false -> BrowserToolbar(shouldShowDivider, settings.shouldUseBottomToolbar)
                }
            }
        }
        homeBinding.homeLayout.addView(this)
    }

    override fun build(browserState: BrowserState) {
        layout.updateLayoutParams {
            (this as? CoordinatorLayout.LayoutParams)?.gravity = when (settings.toolbarPosition) {
                TOP -> Gravity.TOP
                BOTTOM -> Gravity.BOTTOM
            }
        }

        updateHomeAppBarIntegration()
    }

    override fun updateDividerVisibility(isVisible: Boolean) {
        showDivider = isVisible
    }

    override fun updateButtonVisibility(
        browserState: BrowserState,
        shouldAddNavigationBar: Boolean,
    ) {
        // To be added later
    }

    override fun updateTabCounter(browserState: BrowserState) {
        // To be added later
    }

    @Composable
    private fun BrowserToolbar(shouldShowDivider: Boolean, shouldUseBottomToolbar: Boolean) {
        // Ensure the divider is shown together with the toolbar
        Box {
            BrowserToolbar(
                store = BrowserToolbarStore(),
                browserStore = context.components.core.store,
                onDisplayToolbarClick = {},
                onTextEdit = {},
                onTextCommit = {},
                target = Target.Tab("none"),
            )
            if (shouldShowDivider) {
                Divider(
                    modifier = Modifier.align(
                        when (shouldUseBottomToolbar) {
                            true -> Alignment.TopCenter
                            false -> Alignment.BottomCenter
                        },
                    ),
                )
            }
        }
    }

    private fun updateHomeAppBarIntegration() {
        if (!settings.shouldUseBottomToolbar) {
            homeBinding.homeAppBar.updateLayoutParams<ViewGroup.MarginLayoutParams> {
                topMargin = context.resources.getDimensionPixelSize(R.dimen.home_fragment_top_toolbar_header_margin) +
                    when (context.isTabStripEnabled()) {
                        true -> context.resources.getDimensionPixelSize(R.dimen.tab_strip_height)
                        false -> 0
                    }
            }
        }
    }
}
