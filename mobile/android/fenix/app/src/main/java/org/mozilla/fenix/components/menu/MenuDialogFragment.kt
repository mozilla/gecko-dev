/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu

import android.app.AlertDialog
import android.app.Dialog
import android.app.PendingIntent
import android.content.Intent
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.animation.SizeTransform
import androidx.compose.animation.togetherWith
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.ViewCompositionStrategy
import androidx.core.content.ContextCompat
import androidx.core.net.toUri
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.lifecycleScope
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.browser.state.selector.findCustomTab
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.concept.engine.translate.TranslationSupport
import mozilla.components.concept.engine.translate.findLanguage
import mozilla.components.lib.state.ext.observeAsState
import mozilla.components.support.ktx.android.util.dpToPx
import mozilla.components.support.ktx.android.view.setNavigationBarColorCompat
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.components
import org.mozilla.fenix.components.menu.compose.CUSTOM_TAB_MENU_ROUTE
import org.mozilla.fenix.components.menu.compose.CustomTabMenu
import org.mozilla.fenix.components.menu.compose.EXTENSIONS_MENU_ROUTE
import org.mozilla.fenix.components.menu.compose.ExtensionsSubmenu
import org.mozilla.fenix.components.menu.compose.MAIN_MENU_ROUTE
import org.mozilla.fenix.components.menu.compose.MainMenu
import org.mozilla.fenix.components.menu.compose.MainMenuWithCFR
import org.mozilla.fenix.components.menu.compose.MenuDialogBottomSheet
import org.mozilla.fenix.components.menu.compose.SAVE_MENU_ROUTE
import org.mozilla.fenix.components.menu.compose.SaveSubmenu
import org.mozilla.fenix.components.menu.compose.TOOLS_MENU_ROUTE
import org.mozilla.fenix.components.menu.compose.ToolsSubmenu
import org.mozilla.fenix.components.menu.middleware.MenuDialogMiddleware
import org.mozilla.fenix.components.menu.middleware.MenuNavigationMiddleware
import org.mozilla.fenix.components.menu.middleware.MenuTelemetryMiddleware
import org.mozilla.fenix.components.menu.store.BrowserMenuState
import org.mozilla.fenix.components.menu.store.MenuAction
import org.mozilla.fenix.components.menu.store.MenuState
import org.mozilla.fenix.components.menu.store.MenuStore
import org.mozilla.fenix.ext.runIfFragmentIsAttached
import org.mozilla.fenix.nimbus.FxNimbus
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.settings.deletebrowsingdata.deleteAndQuit
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.utils.contentGrowth
import org.mozilla.fenix.utils.enterMenu
import org.mozilla.fenix.utils.enterSubmenu
import org.mozilla.fenix.utils.exitMenu
import org.mozilla.fenix.utils.exitSubmenu

// EXPANDED_MIN_RATIO is used for BottomSheetBehavior.halfExpandedRatio().
// That value needs to be less than the PEEK_HEIGHT.
// If EXPANDED_MIN_RATIO is greater than the PEEK_HEIGHT, then there will be
// three states instead of the expected two states required by design.
private const val PEEK_HEIGHT = 460
private const val EXPANDED_MIN_RATIO = 0.0001f
private const val TOP_EXPANDED_OFFSET = 80
private const val HIDING_FRICTION = 0.9f

/**
 * A bottom sheet fragment displaying the menu dialog.
 */
class MenuDialogFragment : BottomSheetDialogFragment() {

    private val args by navArgs<MenuDialogFragmentArgs>()
    private val browsingModeManager get() = (activity as HomeActivity).browsingModeManager

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        Events.toolbarMenuVisible.record(NoExtras())

        return super.onCreateDialog(savedInstanceState).apply {
            setOnShowListener {
                val navigationBarColor = if (browsingModeManager.mode.isPrivate) {
                    ContextCompat.getColor(context, R.color.fx_mobile_private_layer_color_3)
                } else {
                    ContextCompat.getColor(context, R.color.fx_mobile_layer_color_3)
                }

                window?.setNavigationBarColorCompat(navigationBarColor)

                val bottomSheet = findViewById<View?>(R.id.design_bottom_sheet)
                bottomSheet?.setBackgroundResource(android.R.color.transparent)
                BottomSheetBehavior.from(bottomSheet).apply {
                    isFitToContents = true
                    peekHeight = PEEK_HEIGHT.dpToPx(resources.displayMetrics)
                    halfExpandedRatio = EXPANDED_MIN_RATIO
                    expandedOffset = TOP_EXPANDED_OFFSET
                    state = BottomSheetBehavior.STATE_COLLAPSED
                    hideFriction = HIDING_FRICTION
                }
            }
        }
    }

    @Suppress("LongMethod", "CyclomaticComplexMethod")
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View = ComposeView(requireContext()).apply {
        setViewCompositionStrategy(ViewCompositionStrategy.DisposeOnViewTreeLifecycleDestroyed)

        setContent {
            FirefoxTheme {
                MenuDialogBottomSheet(onRequestDismiss = { dismiss() }) {
                    val appStore = components.appStore
                    val browserStore = components.core.store
                    val syncStore = components.backgroundServices.syncStore
                    val addonManager = components.addonManager
                    val bookmarksStorage = components.core.bookmarksStorage
                    val pinnedSiteStorage = components.core.pinnedSiteStorage
                    val tabCollectionStorage = components.core.tabCollectionStorage
                    val addBookmarkUseCase = components.useCases.bookmarksUseCases.addBookmark
                    val addPinnedSiteUseCase = components.useCases.topSitesUseCase.addPinnedSites
                    val removePinnedSiteUseCase = components.useCases.topSitesUseCase.removeTopSites
                    val topSitesMaxLimit = components.settings.topSitesMaxLimit
                    val appLinksUseCases = components.useCases.appLinksUseCases
                    val webAppUseCases = components.useCases.webAppUseCases
                    val printContentUseCase = components.useCases.sessionUseCases.printContent
                    val requestDesktopSiteUseCase =
                        components.useCases.sessionUseCases.requestDesktopSite
                    val saveToPdfUseCase = components.useCases.sessionUseCases.saveToPdf
                    val selectedTab = browserStore.state.selectedTab
                    val isTranslationEngineSupported =
                        browserStore.state.translationEngine.isEngineSupported ?: false
                    val isTranslationSupported =
                        isTranslationEngineSupported &&
                            FxNimbus.features.translations.value().mainFlowBrowserMenuEnabled
                    val isReaderable = selectedTab?.readerState?.readerable ?: false
                    val settings = components.settings
                    val supportedLanguages = components.core.store.state.translationEngine.supportedLanguages
                    val translateLanguageCode = selectedTab?.translationsState?.translationEngineState
                        ?.requestedTranslationPair?.toLanguage
                    val isExtensionsProcessDisabled = browserStore.state.extensionsProcessDisabled

                    val customTab = args.customTabSessionId?.let {
                        browserStore.state.findCustomTab(it)
                    }

                    val navHostController = rememberNavController()
                    val coroutineScope = rememberCoroutineScope()
                    val store = remember {
                        MenuStore(
                            initialState = MenuState(
                                browserMenuState = if (selectedTab != null) {
                                    BrowserMenuState(selectedTab = selectedTab)
                                } else {
                                    null
                                },
                                customTabSessionId = args.customTabSessionId,
                                isDesktopMode = when (args.accesspoint) {
                                    MenuAccessPoint.Home -> {
                                        settings.openNextTabInDesktopMode
                                    }
                                    MenuAccessPoint.External -> {
                                        customTab?.content?.desktopMode ?: false
                                    }
                                    else -> {
                                        selectedTab?.content?.desktopMode ?: false
                                    }
                                },
                            ),
                            middleware = listOf(
                                MenuDialogMiddleware(
                                    appStore = appStore,
                                    addonManager = addonManager,
                                    settings = settings,
                                    bookmarksStorage = bookmarksStorage,
                                    pinnedSiteStorage = pinnedSiteStorage,
                                    appLinksUseCases = appLinksUseCases,
                                    addBookmarkUseCase = addBookmarkUseCase,
                                    addPinnedSiteUseCase = addPinnedSiteUseCase,
                                    removePinnedSitesUseCase = removePinnedSiteUseCase,
                                    requestDesktopSiteUseCase = requestDesktopSiteUseCase,
                                    alertDialogBuilder = AlertDialog.Builder(context),
                                    topSitesMaxLimit = topSitesMaxLimit,
                                    onDeleteAndQuit = {
                                        deleteAndQuit(
                                            activity = activity as HomeActivity,
                                            // This menu's coroutineScope would cancel all in progress operations
                                            // when the dialog is closed.
                                            // Need to use a scope that will ensure the background operation
                                            // will continue even if the dialog is closed.
                                            coroutineScope = (activity as LifecycleOwner).lifecycleScope,
                                        )
                                    },
                                    onDismiss = {
                                        withContext(Dispatchers.Main) {
                                            this@MenuDialogFragment.dismiss()
                                        }
                                    },
                                    onSendPendingIntentWithUrl = ::sendPendingIntentWithUrl,
                                    scope = coroutineScope,
                                ),
                                MenuNavigationMiddleware(
                                    navController = findNavController(),
                                    navHostController = navHostController,
                                    browsingModeManager = browsingModeManager,
                                    openToBrowser = ::openToBrowser,
                                    webAppUseCases = webAppUseCases,
                                    settings = settings,
                                    onDismiss = {
                                        withContext(Dispatchers.Main) {
                                            this@MenuDialogFragment.dismiss()
                                        }
                                    },
                                    scope = coroutineScope,
                                ),
                                MenuTelemetryMiddleware(
                                    accessPoint = args.accesspoint,
                                ),
                            ),
                        )
                    }
                    val isDesktopMode by store.observeAsState(initialValue = false) { state ->
                        state.isDesktopMode
                    }
                    val recommendedAddons by store.observeAsState(initialValue = emptyList()) { state ->
                        state.extensionMenuState.recommendedAddons
                    }
                    val isBookmarked by store.observeAsState(initialValue = false) { state ->
                        state.browserMenuState != null && state.browserMenuState.bookmarkState.isBookmarked
                    }
                    val isPinned by store.observeAsState(initialValue = false) { state ->
                        state.browserMenuState != null && state.browserMenuState.isPinned
                    }

                    val isReaderViewActive by store.observeAsState(initialValue = false) { state ->
                        state.browserMenuState != null && state.browserMenuState.selectedTab.readerState.active
                    }

                    NavHost(
                        navController = navHostController,
                        startDestination = when (args.accesspoint) {
                            MenuAccessPoint.Browser,
                            MenuAccessPoint.Home,
                            -> MAIN_MENU_ROUTE

                            MenuAccessPoint.External -> CUSTOM_TAB_MENU_ROUTE
                        },
                    ) {
                        composable(
                            route = MAIN_MENU_ROUTE,
                            enterTransition = {
                                (
                                    enterMenu().togetherWith(
                                        exitSubmenu(),
                                    ) using SizeTransform { initialSize, targetSize ->
                                        contentGrowth(initialSize, targetSize)
                                    }
                                    ).targetContentEnter
                            },
                            popEnterTransition = {
                                (
                                    enterMenu().togetherWith(
                                        exitSubmenu(),
                                    ) using SizeTransform { initialSize, targetSize ->
                                        contentGrowth(initialSize, targetSize)
                                    }
                                    ).targetContentEnter
                            },
                            exitTransition = {
                                (
                                    enterSubmenu().togetherWith(
                                        exitMenu(),
                                    ) using SizeTransform { initialSize, targetSize ->
                                        contentGrowth(initialSize, targetSize)
                                    }
                                    ).initialContentExit
                            },
                            popExitTransition = {
                                (
                                    enterSubmenu().togetherWith(
                                        exitMenu(),
                                    ) using SizeTransform { initialSize, targetSize ->
                                        contentGrowth(initialSize, targetSize)
                                    }
                                    ).initialContentExit
                            },
                        ) {
                            if (settings.shouldShowMenuCFR) {
                                MainMenuWithCFR(
                                    accessPoint = args.accesspoint,
                                    store = store,
                                    syncStore = syncStore,
                                    showQuitMenu = settings.shouldDeleteBrowsingDataOnQuit,
                                    isPrivate = browsingModeManager.mode.isPrivate,
                                    isDesktopMode = isDesktopMode,
                                    isTranslationSupported = isTranslationSupported,
                                    isExtensionsProcessDisabled = isExtensionsProcessDisabled,
                                )
                            } else {
                                MainMenu(
                                    accessPoint = args.accesspoint,
                                    store = store,
                                    syncStore = syncStore,
                                    showQuitMenu = settings.shouldDeleteBrowsingDataOnQuit,
                                    isPrivate = browsingModeManager.mode.isPrivate,
                                    isDesktopMode = isDesktopMode,
                                    isTranslationSupported = isTranslationSupported,
                                    isExtensionsProcessDisabled = isExtensionsProcessDisabled,
                                )
                            }
                        }

                        composable(
                            route = TOOLS_MENU_ROUTE,
                            enterTransition = {
                                (
                                    enterSubmenu().togetherWith(
                                        exitMenu(),
                                    ) using SizeTransform { initialSize, targetSize ->
                                        contentGrowth(initialSize, targetSize)
                                    }
                                    ).targetContentEnter
                            },
                            popEnterTransition = {
                                (
                                    enterSubmenu().togetherWith(
                                        exitMenu(),
                                    ) using SizeTransform { initialSize, targetSize ->
                                        contentGrowth(initialSize, targetSize)
                                    }
                                    ).targetContentEnter
                            },
                            exitTransition = {
                                (
                                    enterMenu().togetherWith(
                                        exitSubmenu(),
                                    ) using SizeTransform { initialSize, targetSize ->
                                        contentGrowth(initialSize, targetSize)
                                    }
                                    ).initialContentExit
                            },
                            popExitTransition = {
                                (
                                    enterMenu().togetherWith(
                                        exitSubmenu(),
                                    ) using SizeTransform { initialSize, targetSize ->
                                        contentGrowth(initialSize, targetSize)
                                    }
                                    ).initialContentExit
                            },
                        ) {
                            val appLinksRedirect = if (selectedTab?.content?.url != null) {
                                appLinksUseCases.appLinkRedirect(selectedTab.content.url)
                            } else {
                                null
                            }

                            ToolsSubmenu(
                                isReaderable = isReaderable,
                                isReaderViewActive = isReaderViewActive,
                                hasExternalApp = appLinksRedirect?.hasExternalApp() ?: false,
                                externalAppName = appLinksRedirect?.appName ?: "",
                                isTranslated = selectedTab?.translationsState?.isTranslated ?: false,
                                isTranslationSupported = isTranslationSupported,
                                translatedLanguage = if (translateLanguageCode != null && supportedLanguages != null) {
                                    TranslationSupport(
                                        fromLanguages = supportedLanguages.fromLanguages,
                                        toLanguages = supportedLanguages.toLanguages,
                                    ).findLanguage(translateLanguageCode)?.localizedDisplayName ?: ""
                                } else {
                                    ""
                                },
                                onBackButtonClick = {
                                    store.dispatch(MenuAction.Navigate.Back)
                                },
                                onReaderViewMenuClick = {
                                    store.dispatch(MenuAction.ToggleReaderView)
                                },
                                onCustomizeReaderViewMenuClick = {
                                    store.dispatch(MenuAction.CustomizeReaderView)
                                },
                                onTranslatePageMenuClick = {
                                    selectedTab?.let {
                                        store.dispatch(MenuAction.Navigate.Translate)
                                    }
                                },
                                onPrintMenuClick = {
                                    printContentUseCase()
                                    dismiss()
                                },
                                onShareMenuClick = {
                                    selectedTab?.let {
                                        store.dispatch(MenuAction.Navigate.Share)
                                    }
                                },
                                onOpenInAppMenuClick = {
                                    store.dispatch(MenuAction.OpenInApp)
                                },
                            )
                        }

                        composable(
                            route = SAVE_MENU_ROUTE,
                            enterTransition = {
                                (
                                    enterSubmenu().togetherWith(
                                        exitMenu(),
                                    ) using SizeTransform { initialSize, targetSize ->
                                        contentGrowth(initialSize, targetSize)
                                    }
                                    ).targetContentEnter
                            },
                            popEnterTransition = {
                                (
                                    enterSubmenu().togetherWith(
                                        exitMenu(),
                                    ) using SizeTransform { initialSize, targetSize ->
                                        contentGrowth(initialSize, targetSize)
                                    }
                                    ).targetContentEnter
                            },
                            exitTransition = {
                                (
                                    enterMenu().togetherWith(
                                        exitSubmenu(),
                                    ) using SizeTransform { initialSize, targetSize ->
                                        contentGrowth(initialSize, targetSize)
                                    }
                                    ).initialContentExit
                            },
                            popExitTransition = {
                                (
                                    enterMenu().togetherWith(
                                        exitSubmenu(),
                                    ) using SizeTransform { initialSize, targetSize ->
                                        contentGrowth(initialSize, targetSize)
                                    }
                                    ).initialContentExit
                            },
                        ) {
                            SaveSubmenu(
                                isBookmarked = isBookmarked,
                                isPinned = isPinned,
                                onBackButtonClick = {
                                    store.dispatch(MenuAction.Navigate.Back)
                                },
                                onBookmarkPageMenuClick = {
                                    store.dispatch(MenuAction.AddBookmark)
                                },
                                onEditBookmarkButtonClick = {
                                    store.dispatch(MenuAction.Navigate.EditBookmark)
                                },
                                onShortcutsMenuClick = {
                                    if (!isPinned) {
                                        store.dispatch(MenuAction.AddShortcut)
                                    } else {
                                        store.dispatch(MenuAction.RemoveShortcut)
                                    }
                                },
                                onAddToHomeScreenMenuClick = {
                                    store.dispatch(MenuAction.Navigate.AddToHomeScreen)
                                },
                                onSaveToCollectionMenuClick = {
                                    store.dispatch(
                                        MenuAction.Navigate.SaveToCollection(
                                            hasCollection = tabCollectionStorage
                                                .cachedTabCollections.isNotEmpty(),
                                        ),
                                    )
                                },
                                onSaveAsPDFMenuClick = {
                                    saveToPdfUseCase()
                                    dismiss()
                                },
                            )
                        }

                        composable(
                            route = EXTENSIONS_MENU_ROUTE,
                            enterTransition = {
                                (
                                    enterSubmenu().togetherWith(
                                        exitMenu(),
                                    ) using SizeTransform { initialSize, targetSize ->
                                        contentGrowth(initialSize, targetSize)
                                    }
                                    ).targetContentEnter
                            },
                            popEnterTransition = {
                                (
                                    enterSubmenu().togetherWith(
                                        exitMenu(),
                                    ) using SizeTransform { initialSize, targetSize ->
                                        contentGrowth(initialSize, targetSize)
                                    }
                                    ).targetContentEnter
                            },
                            exitTransition = {
                                (
                                    enterMenu().togetherWith(
                                        exitSubmenu(),
                                    ) using SizeTransform { initialSize, targetSize ->
                                        contentGrowth(initialSize, targetSize)
                                    }
                                    ).initialContentExit
                            },
                            popExitTransition = {
                                (
                                    enterMenu().togetherWith(
                                        exitSubmenu(),
                                    ) using SizeTransform { initialSize, targetSize ->
                                        contentGrowth(initialSize, targetSize)
                                    }
                                    ).initialContentExit
                            },
                        ) {
                            ExtensionsSubmenu(
                                recommendedAddons = recommendedAddons,
                                showExtensionsOnboarding = recommendedAddons.isNotEmpty(),
                                onBackButtonClick = {
                                    store.dispatch(MenuAction.Navigate.Back)
                                },
                                onExtensionsLearnMoreClick = {
                                    store.dispatch(MenuAction.Navigate.ExtensionsLearnMore)
                                },
                                onManageExtensionsMenuClick = {
                                    store.dispatch(MenuAction.Navigate.ManageExtensions)
                                },
                                onAddonClick = { addon ->
                                    store.dispatch(MenuAction.Navigate.AddonDetails(addon = addon))
                                },
                                onInstallAddonClick = { addon ->
                                    store.dispatch(MenuAction.InstallAddon(addon = addon))
                                },
                                onDiscoverMoreExtensionsMenuClick = {
                                    store.dispatch(MenuAction.Navigate.DiscoverMoreExtensions)
                                },
                            )
                        }

                        composable(route = CUSTOM_TAB_MENU_ROUTE) {
                            CustomTabMenu(
                                isDesktopMode = isDesktopMode,
                                customTabMenuItems = customTab?.config?.menuItems,
                                onCustomMenuItemClick = { intent: PendingIntent ->
                                    store.dispatch(
                                        MenuAction.CustomMenuItemAction(
                                            intent = intent,
                                            url = customTab?.content?.url,
                                        ),
                                    )
                                },
                                onSwitchToDesktopSiteMenuClick = {
                                    if (isDesktopMode) {
                                        store.dispatch(MenuAction.RequestMobileSite)
                                    } else {
                                        store.dispatch(MenuAction.RequestDesktopSite)
                                    }
                                },
                                onFindInPageMenuClick = {
                                    store.dispatch(MenuAction.FindInPage)
                                },
                                onOpenInFirefoxMenuClick = {
                                    store.dispatch(MenuAction.OpenInFirefox)
                                },
                            )
                        }
                    }
                }
            }
        }
    }

    private fun openToBrowser(params: BrowserNavigationParams) = runIfFragmentIsAttached {
        val url = params.url ?: params.sumoTopic?.let {
            SupportUtils.getSumoURLForTopic(
                context = requireContext(),
                topic = it,
            )
        }

        url?.let {
            (activity as HomeActivity).openToBrowserAndLoad(
                searchTermOrURL = url,
                newTab = true,
                from = BrowserDirection.FromMenuDialogFragment,
            )
        }
    }

    private fun sendPendingIntentWithUrl(intent: PendingIntent, url: String?) = runIfFragmentIsAttached {
        url?.let { url ->
            intent.send(
                requireContext(),
                0,
                Intent(null, url.toUri()),
            )
        }
    }
}
