/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import android.view.Gravity
import android.view.ViewGroup
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ComposeView
import androidx.coordinatorlayout.widget.CoordinatorLayout.LayoutParams
import mozilla.components.browser.state.helper.Target
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.compose.base.Divider
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.compose.browser.toolbar.BrowserToolbar
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarStore
import org.mozilla.fenix.components.toolbar.ToolbarPosition.BOTTOM
import org.mozilla.fenix.components.toolbar.ToolbarPosition.TOP
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.utils.Settings

/**
 * A wrapper over the [BrowserToolbar] composable to allow for extra customisation and
 * integration in the same framework as the [BrowserToolbarView]
 *
 * @param context [Context] used for various system interactions.
 * @param container [ViewGroup] which will serve as parent of this View.
 * @param settings [Settings] object to get the toolbar position and other settings.
 * @param customTabSession [CustomTabSessionState] if the toolbar is shown in a custom tab.
 * @param tabStripContent Composable content for the tab strip.
 */
class BrowserToolbarComposable(
    private val context: Context,
    container: ViewGroup,
    settings: Settings,
    customTabSession: CustomTabSessionState? = null,
    private val tabStripContent: @Composable () -> Unit,
) : FenixBrowserToolbarView(
    context = context,
    settings = settings,
    customTabSession = customTabSession,
) {
    private var showDivider by mutableStateOf(true)

    override val layout = ComposeView(context).apply {
        setContent {
            val shouldShowDivider by remember { mutableStateOf(showDivider) }
            val shouldShowTabStrip: Boolean = remember { shouldShowTabStrip() }

            AcornTheme {
                when (shouldShowTabStrip) {
                    true -> Column(
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        tabStripContent()
                        BrowserToolbar(shouldShowDivider, settings.shouldUseBottomToolbar)
                    }

                    false -> BrowserToolbar(shouldShowDivider, settings.shouldUseBottomToolbar)
                }
            }
        }

        if (!shouldShowTabStrip()) {
            val params = LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT)

            when (settings.toolbarPosition) {
                TOP -> params.gravity = Gravity.TOP
                BOTTOM -> params.gravity = Gravity.BOTTOM
            }

            layoutParams = params
        }
    }

    init {
        container.addView(layout)
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
                target = Target.SelectedTab,
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

    override fun updateDividerVisibility(isVisible: Boolean) {
        showDivider = isVisible
    }
}
