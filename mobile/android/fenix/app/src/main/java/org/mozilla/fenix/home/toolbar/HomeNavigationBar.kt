/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.toolbar

import android.app.ActionBar.LayoutParams
import android.content.Context
import android.view.ViewGroup
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.runtime.Composable
import androidx.compose.runtime.SideEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.unit.dp
import androidx.fragment.app.Fragment
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.compose.browser.toolbar.NavigationBar
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarState
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarStore
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.utils.Settings

/**
 * A wrapper over the [NavigationBar] composable that provides enhanced customization and
 * lifecycle-aware integration for use within the [FenixHomeToolbar] framework.
 *
 * @param context [Context] used to access resources and other application-level operations.
 * @param lifecycleOwner [Fragment] as a [LifecycleOwner] to used to organize lifecycle dependent operations.
 * @param container [ViewGroup] which will serve as parent of this View.
 * @param appStore [AppStore] to sync from.
 * @param browserStore [BrowserStore] used for observing the browsing details.
 * @param settings [Settings] object to get the toolbar position and other settings.
 */
class HomeNavigationBar(
    private val context: Context,
    private val lifecycleOwner: Fragment,
    private val container: ViewGroup,
    private val appStore: AppStore,
    private val browserStore: BrowserStore,
    private val settings: Settings,
) : FenixHomeToolbar {
    val store = StoreProvider.get(lifecycleOwner) {
        BrowserToolbarStore(
            initialState = BrowserToolbarState(),
            middleware = listOf(
                BrowserToolbarMiddleware(
                    appStore = appStore,
                    browserStore = browserStore,
                    clipboard = context.components.clipboardHandler,
                    useCases = context.components.useCases,
                ),
            ),
        )
    }

    @Composable
    private fun DefaultNavigationBarContent(showDivider: Boolean) {
        if (settings.shouldUseSimpleToolbar) {
            return
        }

        val uiState by store.observeAsState(initialValue = store.state) { it }

        AcornTheme {
            NavigationBar(
                actions = uiState.displayState.navigationActions,
                shouldShowDivider = showDivider,
                onInteraction = { store.dispatch(it) },
            )
        }
    }

    override val layout = ComposeView(context).apply {
        setContent {
            if (settings.shouldUseSimpleToolbar) {
                // empty spacer to be added
                Spacer(modifier = Modifier.height(0.dp))
            } else {
                DefaultNavigationBarContent(showDivider = true)
            }
        }
    }.apply {
        container.addView(
            this,
            LayoutParams(
                LayoutParams.MATCH_PARENT,
                LayoutParams.WRAP_CONTENT,
            ),
        )
    }

    /**
     * Returns a [Composable] function that renders the default navigation bar content and ensures
     * that the associated view-based layout is removed from its parent to prevent UI overlap.
     */
    fun asComposable(): @Composable () -> Unit = {
        val removed = remember { mutableStateOf(false) }

        if (!removed.value) {
            SideEffect {
                (layout.parent as? ViewGroup)?.removeView(layout)
                removed.value = true
            }
        }

        DefaultNavigationBarContent(showDivider = false)
    }

    override fun updateDividerVisibility(isVisible: Boolean) {
        // no-op
    }

    override fun updateButtonVisibility(browserState: BrowserState) {
        // no-op
    }

    override fun updateAddressBarVisibility(isVisible: Boolean) {
        // no-op
    }

    override fun updateTabCounter(browserState: BrowserState) {
        // no-op
    }

    override fun build(browserState: BrowserState) {
        // no-op
    }
}
