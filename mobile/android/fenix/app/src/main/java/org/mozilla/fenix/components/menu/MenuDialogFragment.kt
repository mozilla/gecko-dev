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
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.ViewCompositionStrategy
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
import mozilla.components.service.fxa.manager.AccountState.NotAuthenticated
import mozilla.components.support.ktx.android.util.dpToPx
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.components
import org.mozilla.fenix.components.menu.compose.CUSTOM_TAB_MENU_ROUTE
import org.mozilla.fenix.components.menu.compose.CustomTabMenu
import org.mozilla.fenix.components.menu.compose.EXTENSIONS_MENU_ROUTE
import org.mozilla.fenix.components.menu.compose.ExtensionsSubmenu
import org.mozilla.fenix.components.menu.compose.MAIN_MENU_ROUTE
import org.mozilla.fenix.components.menu.compose.MainMenu
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

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog =
        super.onCreateDialog(savedInstanceState).apply {
            setOnShowListener {
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

    @Suppress("LongMethod", "CyclomaticComplexMethod")
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View = ComposeView(requireContext()).apply {
        setViewCompositionStrategy(ViewCompositionStrategy.DisposeOnViewTreeLifecycleDestroyed)

        setContent {
            FirefoxTheme {
                MenuDialogBottomSheet(onRequestDismiss = {}) {
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
                    val isTranslationSupported = browserStore.state.translationEngine.isEngineSupported ?: false
                    val isReaderable = selectedTab?.readerState?.readerable ?: false
                    val settings = components.settings
                    val supportedLanguages = components.core.store.state.translationEngine.supportedLanguages
                    val translateLanguageCode = selectedTab?.translationsState?.translationEngineState
                        ?.requestedTranslationPair?.toLanguage

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
                                isDesktopMode = if (args.accesspoint == MenuAccessPoint.Home) {
                                    settings.openNextTabInDesktopMode
                                } else {
                                    selectedTab?.content?.desktopMode ?: false
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

                    val account by syncStore.observeAsState(initialValue = null) { state -> state.account }
                    val accountState by syncStore.observeAsState(initialValue = NotAuthenticated) { state ->
                        state.accountState
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
                    val isDesktopMode by store.observeAsState(initialValue = false) { state ->
                        state.isDesktopMode
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
                        composable(route = MAIN_MENU_ROUTE) {
                            MainMenu(
                                accessPoint = args.accesspoint,
                                account = account,
                                accountState = accountState,
                                isPrivate = browsingModeManager.mode.isPrivate,
                                isDesktopMode = isDesktopMode,
                                showQuitMenu = settings.shouldDeleteBrowsingDataOnQuit,
                                onMozillaAccountButtonClick = {
                                    store.dispatch(
                                        MenuAction.Navigate.MozillaAccount(
                                            accountState = accountState,
                                            accesspoint = args.accesspoint,
                                        ),
                                    )
                                },
                                onHelpButtonClick = {
                                    store.dispatch(MenuAction.Navigate.Help)
                                },
                                onSettingsButtonClick = {
                                    store.dispatch(MenuAction.Navigate.Settings)
                                },
                                onNewTabMenuClick = {
                                    store.dispatch(MenuAction.Navigate.NewTab)
                                },
                                onNewPrivateTabMenuClick = {
                                    store.dispatch(MenuAction.Navigate.NewPrivateTab)
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
                                onToolsMenuClick = {
                                    store.dispatch(MenuAction.Navigate.Tools)
                                },
                                onSaveMenuClick = {
                                    store.dispatch(MenuAction.Navigate.Save)
                                },
                                onExtensionsMenuClick = {
                                    store.dispatch(MenuAction.Navigate.Extensions)
                                },
                                onBookmarksMenuClick = {
                                    store.dispatch(MenuAction.Navigate.Bookmarks)
                                },
                                onHistoryMenuClick = {
                                    store.dispatch(MenuAction.Navigate.History)
                                },
                                onDownloadsMenuClick = {
                                    store.dispatch(MenuAction.Navigate.Downloads)
                                },
                                onPasswordsMenuClick = {
                                    store.dispatch(MenuAction.Navigate.Passwords)
                                },
                                onCustomizeHomepageMenuClick = {
                                    store.dispatch(MenuAction.Navigate.CustomizeHomepage)
                                },
                                onNewInFirefoxMenuClick = {
                                    store.dispatch(MenuAction.Navigate.ReleaseNotes)
                                },
                                onQuitMenuClick = {
                                    store.dispatch(MenuAction.DeleteBrowsingDataAndQuit)
                                },
                            )
                        }

                        composable(route = TOOLS_MENU_ROUTE) {
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
                                isTranslationSupported = isTranslationSupported &&
                                    FxNimbus.features.translations.value().mainFlowBrowserMenuEnabled,
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

                        composable(route = SAVE_MENU_ROUTE) {
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

                        composable(route = EXTENSIONS_MENU_ROUTE) {
                            ExtensionsSubmenu(
                                recommendedAddons = recommendedAddons,
                                onBackButtonClick = {
                                    store.dispatch(MenuAction.Navigate.Back)
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
                            val customTab = args.customTabSessionId?.let {
                                browserStore.state.findCustomTab(it)
                            }

                            CustomTabMenu(
                                customTabMenuItems = customTab?.config?.menuItems,
                                onCustomMenuItemClick = { intent: PendingIntent ->
                                    store.dispatch(
                                        MenuAction.CustomMenuItemAction(
                                            intent = intent,
                                            url = customTab?.content?.url,
                                        ),
                                    )
                                },
                                onSwitchToDesktopSiteMenuClick = {},
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
