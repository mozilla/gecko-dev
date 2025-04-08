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
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.coordinatorlayout.widget.CoordinatorLayout.LayoutParams
import androidx.fragment.app.Fragment
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.ViewModelProvider
import androidx.navigation.NavController
import mozilla.components.browser.state.helper.Target
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.compose.base.Divider
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.compose.browser.toolbar.BrowserToolbar
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarState
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarStore
import mozilla.components.feature.toolbar.ToolbarBehaviorController
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.components.toolbar.ToolbarPosition.BOTTOM
import org.mozilla.fenix.components.toolbar.ToolbarPosition.TOP
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.utils.Settings

/**
 * A wrapper over the [BrowserToolbar] composable to allow for extra customisation and
 * integration in the same framework as the [BrowserToolbarView]
 *
 * @param context [Context] used for various system interactions.
 * @param lifecycleOwner [Fragment] as a [LifecycleOwner] to used to organize lifecycle dependent operations.
 * @param container [ViewGroup] which will serve as parent of this View.
 * @param navController [NavController] to use for navigating to other in-app destinations.
 * @param browserStore [BrowserStore] used for observing the browsing details.
 * @param settings [Settings] object to get the toolbar position and other settings.
 * @param customTabSession [CustomTabSessionState] if the toolbar is shown in a custom tab.
 * @param tabStripContent Composable content for the tab strip.
 */
@Suppress("LongParameterList")
class BrowserToolbarComposable(
    private val context: Context,
    lifecycleOwner: Fragment,
    container: ViewGroup,
    private val navController: NavController,
    private val browserStore: BrowserStore,
    settings: Settings,
    customTabSession: CustomTabSessionState? = null,
    private val tabStripContent: @Composable () -> Unit,
) : FenixBrowserToolbarView(
    context = context,
    settings = settings,
    customTabSession = customTabSession,
) {
    private var showDivider by mutableStateOf(true)

    private val middleware = ViewModelProvider(lifecycleOwner)[BrowserToolbarMiddleware::class.java].also {
        it.updateLifecycleDependencies(navController = navController)
    }

    private val store = StoreProvider.get(lifecycleOwner) {
        BrowserToolbarStore(
            initialState = BrowserToolbarState(),
            middleware = listOf(middleware),
        )
    }

    override val layout = ScrollableToolbarComposeView(context, this) {
        val shouldShowDivider by remember { mutableStateOf(showDivider) }
        val shouldShowTabStrip: Boolean = remember { shouldShowTabStrip() }

        DisposableEffect(context) {
            val toolbarController = ToolbarBehaviorController(
                toolbar = this@BrowserToolbarComposable,
                store = browserStore,
                customTabId = customTabSession?.id,
            )
            toolbarController.start()
            onDispose { toolbarController.stop() }
        }

        AcornTheme {
            when (shouldShowTabStrip) {
                true -> Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .wrapContentHeight(),
                ) {
                    tabStripContent()
                    BrowserToolbar(shouldShowDivider, settings.shouldUseBottomToolbar)
                }

                false -> BrowserToolbar(shouldShowDivider, settings.shouldUseBottomToolbar)
            }
        }
    }.apply {
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
        setToolbarBehavior()
    }

    @Composable
    private fun BrowserToolbar(shouldShowDivider: Boolean, shouldUseBottomToolbar: Boolean) {
        // Ensure the divider is shown together with the toolbar
        Box {
            BrowserToolbar(
                store = store,
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
