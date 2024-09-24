/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.customtabs

import android.app.Activity
import android.content.Context
import androidx.annotation.ColorInt
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM
import androidx.appcompat.app.AppCompatDelegate.MODE_NIGHT_NO
import androidx.appcompat.app.AppCompatDelegate.MODE_NIGHT_YES
import androidx.appcompat.content.res.AppCompatResources.getDrawable
import androidx.core.graphics.ColorUtils
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.distinctUntilChangedBy
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import mozilla.components.browser.state.selector.findCustomTabOrSelectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.browser.toolbar.display.DisplayToolbar
import mozilla.components.feature.customtabs.CustomTabsColorsConfig
import mozilla.components.feature.customtabs.CustomTabsToolbarButtonConfig
import mozilla.components.feature.customtabs.CustomTabsToolbarFeature
import mozilla.components.feature.customtabs.CustomTabsToolbarListeners
import mozilla.components.feature.tabs.CustomTabsUseCases
import mozilla.components.lib.state.ext.flow
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.utils.ColorUtils.calculateAlphaFromPercentage
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserFragment.Companion.OPEN_IN_ACTION_WEIGHT
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.OrientationMode
import org.mozilla.fenix.components.menu.MenuAccessPoint
import org.mozilla.fenix.components.toolbar.BrowserToolbarView
import org.mozilla.fenix.components.toolbar.ToolbarMenu
import org.mozilla.fenix.components.toolbar.interactor.BrowserToolbarInteractor
import org.mozilla.fenix.components.toolbar.navbar.shouldAddNavigationBar
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.isTablet
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.utils.Settings

@Suppress("LongParameterList")
class CustomTabsIntegration(
    private val context: Context,
    store: BrowserStore,
    private val appStore: AppStore,
    useCases: CustomTabsUseCases,
    private val browserToolbarView: BrowserToolbarView,
    private val sessionId: String,
    private val activity: Activity,
    private val interactor: BrowserToolbarInteractor,
    shouldReverseItems: Boolean,
    isSandboxCustomTab: Boolean,
    private val isPrivate: Boolean,
    isMenuRedesignEnabled: Boolean,
    private val isNavBarEnabled: Boolean,
) : LifecycleAwareFeature, UserInteractionHandler {

    private val toolbar: BrowserToolbar = browserToolbarView.view
    private lateinit var scope: CoroutineScope
    private val isNavBarVisible
        get() = context.shouldAddNavigationBar()

    @VisibleForTesting
    internal var forwardAction: BrowserToolbar.TwoStateButton? = null

    @VisibleForTesting
    internal var backAction: BrowserToolbar.TwoStateButton? = null

    @VisibleForTesting
    internal var openInAction: BrowserToolbar.Button? = null

    init {
        // Remove toolbar shadow
        toolbar.elevation = 0f

        toolbar.display.displayIndicatorSeparator = false
        toolbar.display.indicators = listOf(
            DisplayToolbar.Indicators.SECURITY,
        )

        // If in private mode, override toolbar background to use private color
        // See #5334
        if (isPrivate) {
            toolbar.background = getDrawable(activity, R.drawable.toolbar_background)
        }
    }

    private val customTabToolbarMenu by lazy {
        CustomTabToolbarMenu(
            context,
            store,
            sessionId,
            shouldReverseItems,
            isSandboxCustomTab,
            onItemTapped = interactor::onBrowserToolbarMenuItemTapped,
        )
    }

    private val feature = CustomTabsToolbarFeature(
        store = store,
        toolbar = toolbar,
        sessionId = sessionId,
        useCases = useCases,
        menuBuilder = if (isMenuRedesignEnabled) null else customTabToolbarMenu.menuBuilder,
        menuItemIndex = START_OF_MENU_ITEMS_INDEX,
        window = activity.window,
        customTabsToolbarListeners = CustomTabsToolbarListeners(
            menuListener = {
                interactor.onMenuButtonClicked(
                    accessPoint = MenuAccessPoint.External,
                    customTabSessionId = sessionId,
                )
            },
            shareListener = { interactor.onBrowserToolbarMenuItemTapped(ToolbarMenu.Item.Share) },
            refreshListener = {
                interactor.onBrowserToolbarMenuItemTapped(
                    ToolbarMenu.Item.Reload(
                        bypassCache = false,
                    ),
                )
            },
        ),
        closeListener = { activity.finishAndRemoveTask() },
        appNightMode = activity.settings().getAppNightMode(),
        forceActionButtonTinting = isPrivate,
        customTabsToolbarButtonConfig = CustomTabsToolbarButtonConfig(
            showMenu = !isNavBarEnabled,
            showRefreshButton = isNavBarEnabled,
            allowCustomizingCloseButton = !isNavBarEnabled,
        ),
        customTabsColorsConfig = getCustomTabsColorsConfig(),
    )

    private fun Settings.getAppNightMode() = if (shouldFollowDeviceTheme) {
        MODE_NIGHT_FOLLOW_SYSTEM
    } else {
        if (shouldUseLightTheme) {
            MODE_NIGHT_NO
        } else {
            MODE_NIGHT_YES
        }
    }

    private fun getCustomTabsColorsConfig() = when (activity.settings().navigationToolbarEnabled) {
        true -> CustomTabsColorsConfig(
            updateStatusBarColor = false,
            updateSystemNavigationBarColor = false,
            updateToolbarsColor = !isPrivate,
        )

        false -> CustomTabsColorsConfig(
            updateStatusBarColor = !isPrivate,
            updateSystemNavigationBarColor = !isPrivate,
            updateToolbarsColor = !isPrivate,
        )
    }

    override fun start() {
        feature.start()
        scope = MainScope().apply {
            launch {
                appStore.flow()
                    .distinctUntilChangedBy { it.orientation }
                    .map { it.orientation }
                    .collect {
                        updateToolbarLayout(
                            context = context,
                            isNavBarEnabled = isNavBarEnabled,
                            isNavBarVisible = isNavBarVisible,
                            orientation = it,
                        )
                    }
            }
        }
    }

    override fun stop() {
        feature.stop()
        scope.cancel()
    }

    override fun onBackPressed() = feature.onBackPressed()

    @VisibleForTesting
    internal fun updateToolbarLayout(
        context: Context,
        isNavBarEnabled: Boolean,
        isNavBarVisible: Boolean,
        orientation: OrientationMode,
    ) {
        if (isNavBarEnabled) {
            updateAddressBarNavigationActions(
                context = context,
                isLandscape = orientation == OrientationMode.Landscape,
                isTablet = context.isTablet(),
            )

            browserToolbarView.updateMenuVisibility(
                isVisible = !isNavBarVisible,
            )

            updateOpenInAction(
                isNavbarVisible = isNavBarVisible,
                context = context,
            )
        }
    }

    @VisibleForTesting
    internal fun updateAddressBarNavigationActions(
        context: Context,
        isLandscape: Boolean,
        isTablet: Boolean,
    ) {
        if (isLandscape || isTablet) {
            addNavigationActions(context)
            toolbar.invalidateActions()
        } else {
            removeNavigationActions()
        }
    }

    @VisibleForTesting
    internal fun updateOpenInAction(
        isNavbarVisible: Boolean,
        context: Context,
    ) {
        if (!isNavbarVisible) {
            initOpenInAction(context)
        } else {
            removeOpenInAction()
        }
    }

    @VisibleForTesting
    internal fun addNavigationActions(context: Context) {
        val enableTint = feature.iconColor
        val disableTint = ColorUtils.setAlphaComponent(
            feature.iconColor,
            calculateAlphaFromPercentage(DISABLED_STATE_OPACITY),
        )

        initBackwardAction(
            context = context,
            enableTint = enableTint,
            disableTint = disableTint,
        )

        initForwardAction(
            context = context,
            enableTint = enableTint,
            disableTint = disableTint,
        )
    }

    @VisibleForTesting
    internal fun initForwardAction(
        context: Context,
        @ColorInt enableTint: Int,
        @ColorInt disableTint: Int,
    ) {
        if (forwardAction == null) {
            forwardAction = BrowserToolbar.TwoStateButton(
                primaryImage = getDrawable(
                    context,
                    R.drawable.mozac_ic_forward_24,
                )!!.apply {
                    setTint(enableTint)
                },
                primaryContentDescription = context.getString(R.string.browser_menu_forward),
                secondaryImage = getDrawable(
                    context,
                    R.drawable.mozac_ic_forward_24,
                )!!.apply {
                    setTint(disableTint)
                },
                secondaryContentDescription = context.getString(R.string.browser_menu_forward),
                isInPrimaryState = {
                    val currentTab = context.components.core.store.state.findCustomTabOrSelectedTab(sessionId)
                    currentTab?.content?.canGoForward ?: false
                },
                disableInSecondaryState = true,
                longClickListener = {
                    interactor.onBrowserToolbarMenuItemTapped(
                        ToolbarMenu.Item.Forward(viewHistory = true),
                    )
                },
                listener = {
                    interactor.onBrowserToolbarMenuItemTapped(
                        ToolbarMenu.Item.Forward(viewHistory = false),
                    )
                },
            ).also {
                toolbar.addNavigationAction(it)
            }
        }
    }

    @VisibleForTesting
    internal fun initBackwardAction(
        context: Context,
        @ColorInt enableTint: Int,
        @ColorInt disableTint: Int,
    ) {
        if (backAction == null) {
            backAction = BrowserToolbar.TwoStateButton(
                primaryImage = getDrawable(
                    context,
                    R.drawable.mozac_ic_back_24,
                )!!.apply {
                    setTint(enableTint)
                },
                primaryContentDescription = context.getString(R.string.browser_menu_back),
                secondaryImage = getDrawable(
                    context,
                    R.drawable.mozac_ic_back_24,
                )!!.apply {
                    setTint(disableTint)
                },
                secondaryContentDescription = context.getString(R.string.browser_menu_back),
                isInPrimaryState = {
                    val currentTab = context.components.core.store.state.findCustomTabOrSelectedTab(sessionId)
                    currentTab?.content?.canGoBack ?: false
                },
                disableInSecondaryState = true,
                longClickListener = {
                    interactor.onBrowserToolbarMenuItemTapped(
                        ToolbarMenu.Item.Back(viewHistory = true),
                    )
                },
                listener = {
                    interactor.onBrowserToolbarMenuItemTapped(
                        ToolbarMenu.Item.Back(viewHistory = false),
                    )
                },
            ).also {
                toolbar.addNavigationAction(it)
            }
        }
    }

    @VisibleForTesting
    internal fun removeNavigationActions() {
        forwardAction?.let {
            toolbar.removeNavigationAction(it)
        }
        forwardAction = null

        backAction?.let {
            toolbar.removeNavigationAction(it)
        }
        backAction = null
    }

    @VisibleForTesting
    internal fun initOpenInAction(context: Context) {
        if (openInAction == null) {
            openInAction = BrowserToolbar.Button(
                imageDrawable = getDrawable(
                    context,
                    R.drawable.mozac_ic_open_in,
                )!!.also {
                    it.setTint(feature.iconColor)
                },
                contentDescription = context.getString(
                    R.string.browser_menu_open_in_fenix,
                    context.getString(R.string.app_name),
                ),
                weight = { OPEN_IN_ACTION_WEIGHT },
                listener = {
                    interactor.onBrowserToolbarMenuItemTapped(
                        ToolbarMenu.Item.OpenInFenix,
                    )
                },
            ).also {
                toolbar.addBrowserAction(it)
            }
        }
    }

    @VisibleForTesting
    internal fun removeOpenInAction() {
        openInAction?.let {
            toolbar.removeBrowserAction(it)
        }
        openInAction = null
    }

    companion object {
        private const val START_OF_MENU_ITEMS_INDEX = 2
        private const val DISABLED_STATE_OPACITY = 40
    }
}
