/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.view.Gravity
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.coordinatorlayout.widget.CoordinatorLayout.LayoutParams
import androidx.fragment.app.Fragment
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.ViewModelProvider
import androidx.navigation.NavController
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.thumbnails.BrowserThumbnails
import mozilla.components.compose.base.Divider
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.compose.base.theme.localAcornColors
import mozilla.components.compose.browser.toolbar.BrowserToolbar
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarAction
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarState
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarStore
import mozilla.components.feature.toolbar.ToolbarBehaviorController
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.ext.observeAsComposableState
import org.mozilla.fenix.browser.BrowserAnimator
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.browser.readermode.ReaderModeController
import org.mozilla.fenix.browser.store.BrowserScreenStore
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.components.toolbar.BrowserToolbarMiddleware.LifecycleDependencies
import org.mozilla.fenix.components.toolbar.ToolbarPosition.BOTTOM
import org.mozilla.fenix.components.toolbar.ToolbarPosition.TOP
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.utils.Settings

/**
 * A wrapper over the [BrowserToolbar] composable to allow for extra customisation and
 * integration in the same framework as the [BrowserToolbarView]
 *
 * @param activity [AppCompatActivity] hosting the toolbar.
 * @param lifecycleOwner [Fragment] as a [LifecycleOwner] to used to organize lifecycle dependent operations.
 * @param container [ViewGroup] which will serve as parent of this View.
 * @param navController [NavController] to use for navigating to other in-app destinations.
 * @param appStore [AppStore] to sync from.
 * @param browserScreenStore [BrowserScreenStore] used for integration with other browser screen functionalities.
 * @param browserStore [BrowserStore] used for observing the browsing details.
 * @param components [Components] allowing interactions with other application features.
 * @param browsingModeManager [BrowsingModeManager] for querying the current browsing mode.
 * @param browserAnimator Helper for animating the browser content when navigating to other screens.
 * @param thumbnailsFeature [BrowserThumbnails] for requesting screenshots of the current tab.
 * @param readerModeController [ReaderModeController] for managing the reader mode.
 * @param settings [Settings] object to get the toolbar position and other settings.
 * @param customTabSession [CustomTabSessionState] if the toolbar is shown in a custom tab.
 * @param tabStripContent Composable content for the tab strip.
 */
@Suppress("LongParameterList")
class BrowserToolbarComposable(
    private val activity: AppCompatActivity,
    private val lifecycleOwner: Fragment,
    container: ViewGroup,
    private val navController: NavController,
    private val appStore: AppStore,
    private val browserScreenStore: BrowserScreenStore,
    private val browserStore: BrowserStore,
    private val components: Components,
    private val browsingModeManager: BrowsingModeManager,
    private val browserAnimator: BrowserAnimator,
    private val thumbnailsFeature: BrowserThumbnails?,
    private val readerModeController: ReaderModeController,
    private val settings: Settings,
    private val customTabSession: CustomTabSessionState? = null,
    private val tabStripContent: @Composable () -> Unit,
) : FenixBrowserToolbarView(
    context = activity,
    settings = settings,
    customTabSession = customTabSession,
) {
    private var showDivider by mutableStateOf(false)

    private val middleware: Middleware<BrowserToolbarState, BrowserToolbarAction> = when (customTabSession) {
        null -> getOrCreate<BrowserToolbarMiddleware>()
        else -> getOrCreate<CustomTabBrowserToolbarMiddleware>()
    }
    val store = StoreProvider.get(lifecycleOwner) {
        BrowserToolbarStore(
            initialState = BrowserToolbarState(),
            middleware = listOf(middleware),
        )
    }

    override val layout = ScrollableToolbarComposeView(activity, this) {
        val shouldShowTabStrip: Boolean = remember { shouldShowTabStrip() }
        val progressBarValue = store.observeAsComposableState { it.displayState.progressBarConfig?.progress }.value ?: 0
        val customColors = browserScreenStore.observeAsComposableState { it.customTabColors }

        DisposableEffect(activity) {
            val toolbarController = ToolbarBehaviorController(
                toolbar = this@BrowserToolbarComposable,
                store = browserStore,
                customTabId = customTabSession?.id,
            )
            toolbarController.start()
            onDispose { toolbarController.stop() }
        }

        AcornTheme {
            val acornColors = AcornTheme.colors
            val customTheme = remember(customColors, acornColors) {
                acornColors.copy(
                    // Toolbar background
                    layer1 = customColors.value?.toolbarColor?.let { Color(it) } ?: acornColors.layer1,
                    // Page origin background
                    layer3 = customColors.value?.toolbarColor?.let { Color(it) } ?: acornColors.layer3,
                    // All text but the title
                    textPrimary = customColors.value?.readableColor?.let { Color(it) } ?: acornColors.textPrimary,
                    // Title
                    textSecondary = customColors.value?.readableColor?.let { Color(it) } ?: acornColors.textSecondary,
                    // All icons tint
                    iconPrimary = customColors.value?.readableColor?.let { Color(it) } ?: acornColors.iconPrimary,
                )
            }

            CompositionLocalProvider(localAcornColors provides customTheme) {
                when (shouldShowTabStrip) {
                    true -> Column(
                        modifier = Modifier
                            .fillMaxWidth()
                            .wrapContentHeight(),
                    ) {
                        tabStripContent()
                        BrowserToolbar(showDivider, progressBarValue, settings.shouldUseBottomToolbar)
                    }

                    false -> BrowserToolbar(showDivider, progressBarValue, settings.shouldUseBottomToolbar)
                }
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
        updateDividerVisibility(true)
    }

    @Composable
    private fun BrowserToolbar(
        shouldShowDivider: Boolean,
        progressBarValue: Int,
        shouldUseBottomToolbar: Boolean,
    ) {
        // Ensure the divider is shown together with the toolbar
        Box {
            BrowserToolbar(
                store = store,
            )
            @Suppress("MagicNumber")
            if (shouldShowDivider && progressBarValue !in 1..99) {
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
        showDivider = when (customTabSession) {
            null -> isVisible
            else -> false
        }
    }

    private inline fun <reified T> getOrCreate(): T = when (T::class.java) {
        BrowserToolbarMiddleware::class.java ->
            ViewModelProvider(
                lifecycleOwner,
                BrowserToolbarMiddleware.viewModelFactory(
                    appStore = appStore,
                    browserScreenStore = browserScreenStore,
                    browserStore = browserStore,
                    useCases = components.useCases,
                    clipboard = activity.components.clipboardHandler,
                    settings = settings,
                ),
            ).get(BrowserToolbarMiddleware::class.java).also {
                it.updateLifecycleDependencies(
                    LifecycleDependencies(
                        context = activity,
                        lifecycleOwner = lifecycleOwner,
                        navController = navController,
                        browsingModeManager = browsingModeManager,
                        browserAnimator = browserAnimator,
                        thumbnailsFeature = thumbnailsFeature,
                        readerModeController = readerModeController,
                    ),
                )
            } as T

        CustomTabBrowserToolbarMiddleware::class.java ->
            ViewModelProvider(
                lifecycleOwner,
                CustomTabBrowserToolbarMiddleware.viewModelFactory(
                    requireNotNull(customTabSession).id,
                    browserStore = browserStore,
                    permissionsStorage = components.core.geckoSitePermissionsStorage,
                    cookieBannersStorage = components.core.cookieBannersStorage,
                    useCases = components.useCases.customTabsUseCases,
                    trackingProtectionUseCases = components.useCases.trackingProtectionUseCases,
                    publicSuffixList = components.publicSuffixList,
                    settings = settings,
                ),
            ).get(CustomTabBrowserToolbarMiddleware::class.java).also {
                it.updateLifecycleDependencies(
                    CustomTabBrowserToolbarMiddleware.LifecycleDependencies(
                        context = activity,
                        lifecycleOwner = lifecycleOwner,
                        navController = navController,
                        closeTabDelegate = { activity.finishAndRemoveTask() },
                    ),
                )
            } as T

        else -> throw IllegalArgumentException("Unknown type: ${T::class.java}")
    }
}
