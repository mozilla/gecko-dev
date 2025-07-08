/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import android.view.Gravity
import android.view.ViewGroup
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.SideEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.coordinatorlayout.widget.CoordinatorLayout.LayoutParams
import androidx.fragment.app.Fragment
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.compose.browser.toolbar.NavigationBar
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarState
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarStore
import mozilla.components.feature.toolbar.ToolbarBehaviorController
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.browser.store.BrowserScreenStore
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.utils.Settings

/**
 * A wrapper over the [NavigationBar] composable that provides enhanced customization and
 * lifecycle-aware integration for use within the [BrowserToolbarView] framework.
 *
 * @param context [Context] used to access resources and other application-level operations.
 * @param lifecycleOwner [Fragment] as a [LifecycleOwner] to used to organize lifecycle dependent operations.
 * @param container [ViewGroup] which will serve as parent of this View.
 * @param appStore [AppStore] to sync from.
 * @param browserScreenStore [BrowserScreenStore] used for integration with other browser screen functionalities.
 * @param browserStore [BrowserStore] used for observing the browsing details.
 * @param components [Components] allowing interactions with other application features.
 * @param settings [Settings] object to get the toolbar position and other settings.
 * @param customTabSession [CustomTabSessionState] if the toolbar is shown in a custom tab.
 */
@Suppress("LongParameterList")
class BrowserNavigationBar(
    private val context: Context,
    private val lifecycleOwner: Fragment,
    container: ViewGroup,
    private val appStore: AppStore,
    private val browserScreenStore: BrowserScreenStore,
    private val browserStore: BrowserStore,
    private val components: Components,
    private val settings: Settings,
    customTabSession: CustomTabSessionState? = null,
) : FenixBrowserToolbarView(
    context = context,
    settings = settings,
    customTabSession = customTabSession,
) {
    override fun updateDividerVisibility(isVisible: Boolean) {
        // No-op: Divider is not controlled through this.
    }

    val store = StoreProvider.get(lifecycleOwner) {
        BrowserToolbarStore(
            initialState = BrowserToolbarState(),
            middleware = listOf(
                BrowserToolbarMiddleware(
                    appStore = appStore,
                    browserScreenStore = browserScreenStore,
                    browserStore = browserStore,
                    permissionsStorage = components.core.geckoSitePermissionsStorage,
                    cookieBannersStorage = components.core.cookieBannersStorage,
                    trackingProtectionUseCases = components.useCases.trackingProtectionUseCases,
                    useCases = components.useCases,
                    nimbusComponents = components.nimbus,
                    clipboard = components.clipboardHandler,
                    publicSuffixList = components.publicSuffixList,
                    settings = settings,
                    bookmarksStorage = components.core.bookmarksStorage,
                ),
            ),
        )
    }

    override val layout: ScrollableToolbarComposeView =
        ScrollableToolbarComposeView(context, this) {
            DisposableEffect(browserStore, customTabSession) {
                val toolbarController = ToolbarBehaviorController(
                    toolbar = this@BrowserNavigationBar,
                    store = browserStore,
                    customTabId = customTabSession?.id,
                )
                toolbarController.start()
                onDispose { toolbarController.stop() }
            }

            DefaultNavigationBarContent(showDivider = true)
        }.apply {
            container.addView(
                this,
                LayoutParams(
                    LayoutParams.MATCH_PARENT,
                    LayoutParams.WRAP_CONTENT,
                ).apply {
                    gravity = Gravity.BOTTOM
                },
            )

            post { setToolbarBehavior(ToolbarPosition.BOTTOM) }
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

    @Composable
    private fun DefaultNavigationBarContent(showDivider: Boolean) {
        val uiState by store.observeAsState(initialValue = store.state) { it }

        FirefoxTheme {
            NavigationBar(
                actions = uiState.displayState.navigationActions,
                shouldShowDivider = showDivider,
                onInteraction = { store.dispatch(it) },
            )
        }
    }
}
