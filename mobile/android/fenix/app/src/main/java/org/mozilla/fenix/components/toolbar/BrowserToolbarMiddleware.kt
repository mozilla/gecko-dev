/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import android.os.Build
import androidx.annotation.VisibleForTesting
import androidx.lifecycle.Lifecycle.State.RESUMED
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import androidx.navigation.NavController
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.distinctUntilChangedBy
import kotlinx.coroutines.launch
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.selector.getNormalOrPrivateTabs
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.thumbnails.BrowserThumbnails
import mozilla.components.compose.browser.toolbar.concept.Action
import mozilla.components.compose.browser.toolbar.concept.Action.ActionButton
import mozilla.components.compose.browser.toolbar.concept.Action.ActionButtonRes
import mozilla.components.compose.browser.toolbar.concept.Action.TabCounterAction
import mozilla.components.compose.browser.toolbar.concept.PageOrigin
import mozilla.components.compose.browser.toolbar.concept.PageOrigin.Companion.ContextualMenuOption
import mozilla.components.compose.browser.toolbar.concept.PageOrigin.Companion.PageOriginContextualMenuInteractions.CopyToClipboardClicked
import mozilla.components.compose.browser.toolbar.concept.PageOrigin.Companion.PageOriginContextualMenuInteractions.LoadFromClipboardClicked
import mozilla.components.compose.browser.toolbar.concept.PageOrigin.Companion.PageOriginContextualMenuInteractions.PasteFromClipboardClicked
import mozilla.components.compose.browser.toolbar.store.BrowserDisplayToolbarAction
import mozilla.components.compose.browser.toolbar.store.BrowserDisplayToolbarAction.BrowserActionsEndUpdated
import mozilla.components.compose.browser.toolbar.store.BrowserDisplayToolbarAction.BrowserActionsStartUpdated
import mozilla.components.compose.browser.toolbar.store.BrowserDisplayToolbarAction.PageActionsEndUpdated
import mozilla.components.compose.browser.toolbar.store.BrowserDisplayToolbarAction.UpdateProgressBarConfig
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarAction
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarAction.Init
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarEvent
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarMenu
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton.ContentDescription.StringResContentDescription
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton.Icon.DrawableResIcon
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton.Text.StringResText
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuDivider
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarState
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarStore
import mozilla.components.compose.browser.toolbar.store.ProgressBarConfig
import mozilla.components.compose.browser.toolbar.store.ProgressBarGravity
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.concept.engine.cookiehandling.CookieBannersStorage
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.concept.engine.permission.SitePermissionsStorage
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.session.TrackingProtectionUseCases
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import mozilla.components.lib.state.ext.flow
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.kotlin.getOrigin
import mozilla.components.support.ktx.kotlin.isContentUrl
import mozilla.components.support.ktx.util.URLStringUtils
import mozilla.components.support.utils.ClipboardHandler
import org.mozilla.fenix.GleanMetrics.Translations
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserAnimator
import org.mozilla.fenix.browser.BrowserAnimator.Companion.getToolbarNavOptions
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingMode.Normal
import org.mozilla.fenix.browser.browsingmode.BrowsingMode.Private
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.browser.readermode.ReaderModeController
import org.mozilla.fenix.browser.store.BrowserScreenAction
import org.mozilla.fenix.browser.store.BrowserScreenStore
import org.mozilla.fenix.browser.tabstrip.isTabStripEnabled
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.UseCases
import org.mozilla.fenix.components.appstate.AppAction.CurrentTabClosed
import org.mozilla.fenix.components.appstate.AppAction.SnackbarAction.SnackbarDismissed
import org.mozilla.fenix.components.appstate.AppAction.URLCopiedToClipboard
import org.mozilla.fenix.components.appstate.AppAction.UpdateSearchBeingActiveState
import org.mozilla.fenix.components.menu.MenuAccessPoint
import org.mozilla.fenix.components.toolbar.DisplayActions.MenuClicked
import org.mozilla.fenix.components.toolbar.DisplayActions.NavigateBackClicked
import org.mozilla.fenix.components.toolbar.DisplayActions.NavigateForwardClicked
import org.mozilla.fenix.components.toolbar.DisplayActions.NavigateSessionLongClicked
import org.mozilla.fenix.components.toolbar.DisplayActions.RefreshClicked
import org.mozilla.fenix.components.toolbar.DisplayActions.StopRefreshClicked
import org.mozilla.fenix.components.toolbar.PageEndActionsInteractions.ReaderModeClicked
import org.mozilla.fenix.components.toolbar.PageEndActionsInteractions.TranslateClicked
import org.mozilla.fenix.components.toolbar.PageOriginInteractions.OriginClicked
import org.mozilla.fenix.components.toolbar.TabCounterInteractions.AddNewPrivateTab
import org.mozilla.fenix.components.toolbar.TabCounterInteractions.AddNewTab
import org.mozilla.fenix.components.toolbar.TabCounterInteractions.CloseCurrentTab
import org.mozilla.fenix.components.toolbar.TabCounterInteractions.TabCounterClicked
import org.mozilla.fenix.components.toolbar.URLDomainHighlight.getRegistrableDomainOrHostIndexRange
import org.mozilla.fenix.components.usecases.FenixBrowserUseCases.Companion.ABOUT_HOME
import org.mozilla.fenix.ext.isLargeWindow
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.navigateSafe
import org.mozilla.fenix.settings.quicksettings.protections.cookiebanners.getCookieBannerUIMode
import org.mozilla.fenix.tabstray.Page
import org.mozilla.fenix.tabstray.ext.isActiveDownload
import org.mozilla.fenix.utils.Settings
import mozilla.components.lib.state.Action as MVIAction
import mozilla.components.ui.icons.R as iconsR

@VisibleForTesting
internal sealed class DisplayActions : BrowserToolbarEvent {
    data object MenuClicked : DisplayActions()
    data object NavigateBackClicked : DisplayActions()
    data object NavigateForwardClicked : DisplayActions()
    data object NavigateSessionLongClicked : DisplayActions()
    data class RefreshClicked(
        val bypassCache: Boolean,
    ) : DisplayActions()
    data object StopRefreshClicked : DisplayActions()
}

@VisibleForTesting
internal sealed class StartPageActions : BrowserToolbarEvent {
    data object SiteInfoClicked : StartPageActions()
}

@VisibleForTesting
internal sealed class TabCounterInteractions : BrowserToolbarEvent {
    data object TabCounterClicked : TabCounterInteractions()
    data object AddNewTab : TabCounterInteractions()
    data object AddNewPrivateTab : TabCounterInteractions()
    data object CloseCurrentTab : TabCounterInteractions()
}

@VisibleForTesting
internal sealed class PageOriginInteractions : BrowserToolbarEvent {
    data object OriginClicked : PageOriginInteractions()
}

@VisibleForTesting
internal sealed class PageEndActionsInteractions : BrowserToolbarEvent {
    data class ReaderModeClicked(
        val isActive: Boolean,
    ) : PageEndActionsInteractions()

    data object TranslateClicked : PageEndActionsInteractions()
}

/**
 * [Middleware] responsible for configuring and handling interactions with the composable toolbar.
 *
 * This is also a [ViewModel] allowing to be easily persisted between activity restarts.
 *
 * @param appStore [AppStore] allowing to integrate with other features of the applications.
 * @param browserScreenStore [BrowserScreenStore] used for integration with other browser screen functionalities.
 * @param browserStore [BrowserStore] to sync from.
 * @param permissionsStorage [SitePermissionsStorage] to find currently selected tab site permissions.
 * @param cookieBannersStorage [CookieBannersStorage] to get the current status of cookie banner ui mode.
 * @param trackingProtectionUseCases [TrackingProtectionUseCases] allowing to query
 * tracking protection data of the current tab.
 * @param useCases [UseCases] helping this integrate with other features of the applications.
 * @param clipboard [ClipboardHandler] to use for reading from device's clipboard.
 * @param publicSuffixList [PublicSuffixList] used to obtain the base domain of the current site.
 * @param settings [Settings] for accessing user preferences.
 * @param sessionUseCases [SessionUseCases] for interacting with the current session.
 */
@Suppress("LargeClass", "LongParameterList", "TooManyFunctions")
class BrowserToolbarMiddleware(
    private val appStore: AppStore,
    private val browserScreenStore: BrowserScreenStore,
    private val browserStore: BrowserStore,
    private val permissionsStorage: SitePermissionsStorage,
    private val cookieBannersStorage: CookieBannersStorage,
    private val trackingProtectionUseCases: TrackingProtectionUseCases,
    private val useCases: UseCases,
    private val clipboard: ClipboardHandler,
    private val publicSuffixList: PublicSuffixList,
    private val settings: Settings,
    private val sessionUseCases: SessionUseCases = SessionUseCases(browserStore),
) : Middleware<BrowserToolbarState, BrowserToolbarAction>, ViewModel() {
    private lateinit var dependencies: LifecycleDependencies
    private var store: BrowserToolbarStore? = null

    /**
     * Updates the [LifecycleDependencies] of this middleware.
     *
     * @param dependencies The new [LifecycleDependencies].
     */
    fun updateLifecycleDependencies(dependencies: LifecycleDependencies) {
        this.dependencies = dependencies

        observeProgressBarUpdates()
        updateToolbarActionsBasedOnOrientation()
        observeTabsCountUpdates()
        observeAcceptingCancellingPrivateDownloads()
        observePageNavigationStatus()
        observePageOriginUpdates()
        observeReaderModeUpdates()
        observePageTranslationsUpdates()
        observePageRefreshUpdates()
        observePageSecurityUpdates()
    }

    @Suppress("LongMethod")
    override fun invoke(
        context: MiddlewareContext<BrowserToolbarState, BrowserToolbarAction>,
        next: (BrowserToolbarAction) -> Unit,
        action: BrowserToolbarAction,
    ) {
        when (action) {
            is Init -> {
                store = context.store as BrowserToolbarStore

                appStore.dispatch(UpdateSearchBeingActiveState(context.store.state.isEditMode()))

                updateStartBrowserActions()
                updateCurrentPageOrigin()
                updateEndBrowserActions()
                updateCurrentPageOrigin()
                updateStartPageActions()
            }

            is StartPageActions.SiteInfoClicked -> {
                onSiteInfoClicked()
            }

            is MenuClicked -> {
                dependencies.navController.nav(
                    R.id.browserFragment,
                    BrowserFragmentDirections.actionGlobalMenuDialogFragment(
                        accesspoint = MenuAccessPoint.Browser,
                    ),
                )
            }

            is TabCounterClicked -> {
                dependencies.thumbnailsFeature?.requestScreenshot()

                dependencies.navController.nav(
                    R.id.browserFragment,
                    BrowserFragmentDirections.actionGlobalTabsTrayFragment(
                        page = when (dependencies.browsingModeManager.mode) {
                            Normal -> Page.NormalTabs
                            Private -> Page.PrivateTabs
                        },
                    ),
                )
            }
            is AddNewTab -> {
                openNewTab(Normal)
            }
            is AddNewPrivateTab -> {
                openNewTab(Private)
            }
            is CloseCurrentTab -> {
                browserStore.state.selectedTab?.let { selectedTab ->
                    val isLastTab = browserStore.state.getNormalOrPrivateTabs(selectedTab.content.private).size == 1

                    if (!isLastTab) {
                        useCases.tabsUseCases.removeTab(selectedTab.id, selectParentIfExists = true)
                        appStore.dispatch(CurrentTabClosed(selectedTab.content.private))
                        return@let
                    }

                    if (!selectedTab.content.private) {
                        dependencies.navController.navigate(
                            BrowserFragmentDirections.actionGlobalHome(
                                sessionToDelete = selectedTab.id,
                            ),
                        )
                        return@let
                    }

                    val privateDownloads = browserStore.state.downloads.filter {
                        it.value.private && it.value.isActiveDownload()
                    }
                    if (privateDownloads.isNotEmpty() && !browserScreenStore.state.cancelPrivateDownloadsAccepted) {
                        browserScreenStore.dispatch(
                            BrowserScreenAction.ClosingLastPrivateTab(
                                tabId = selectedTab.id,
                                inProgressPrivateDownloads = privateDownloads.size,
                            ),
                        )
                    } else {
                        dependencies.navController.navigate(
                            BrowserFragmentDirections.actionGlobalHome(
                                sessionToDelete = selectedTab.id,
                            ),
                        )
                    }
                }
            }

            is OriginClicked -> {
                val selectedTab = browserStore.state.selectedTab ?: return
                if (selectedTab.content.searchTerms.isBlank()) {
                    dependencies.navController.navigate(
                        BrowserFragmentDirections.actionGlobalHome(
                            focusOnAddressBar = true,
                            sessionToStartSearchFor = selectedTab.id,
                        ),
                    )
                }
            }
            is CopyToClipboardClicked -> {
                val selectedTab = browserStore.state.selectedTab
                val url = selectedTab?.readerState?.activeUrl ?: selectedTab?.content?.url
                clipboard.text = url

                // Android 13+ shows by default a popup for copied text.
                // Avoid overlapping popups informing the user when the URL is copied to the clipboard.
                // and only show our snackbar when Android will not show an indication by default.
                // See https://developer.android.com/develop/ui/views/touch-and-input/copy-paste#duplicate-notifications).
                if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.S_V2) {
                    appStore.dispatch(URLCopiedToClipboard)
                }
            }
            is PasteFromClipboardClicked -> {
                dependencies.navController.nav(
                    R.id.browserFragment,
                    BrowserFragmentDirections.actionGlobalSearchDialog(
                        sessionId = browserStore.state.selectedTabId,
                        pastedText = clipboard.text,
                    ),
                    getToolbarNavOptions(dependencies.context),
                )
            }
            is LoadFromClipboardClicked -> {
                clipboard.extractURL()?.let {
                    useCases.fenixBrowserUseCases.loadUrlOrSearch(
                        searchTermOrURL = it,
                        newTab = false,
                        private = dependencies.browsingModeManager.mode == Private,
                    )
                } ?: run {
                    Logger("BrowserOriginContextMenu").error("Clipboard contains URL but unable to read text")
                }
            }
            is NavigateSessionLongClicked -> {
                dependencies.navController.nav(
                    R.id.browserFragment,
                    BrowserFragmentDirections.actionGlobalTabHistoryDialogFragment(
                        activeSessionId = null,
                    ),
                )
            }
            is NavigateBackClicked -> {
                browserStore.state.selectedTab?.let {
                    browserStore.dispatch(EngineAction.GoBackAction(it.id))
                }
            }
            is NavigateForwardClicked -> {
                browserStore.state.selectedTab?.let {
                    browserStore.dispatch(EngineAction.GoForwardAction(it.id))
                }
            }

            is ReaderModeClicked -> when (action.isActive) {
                true -> dependencies.readerModeController.hideReaderView()
                false -> dependencies.readerModeController.showReaderView()
            }

            is TranslateClicked -> {
                Translations.action.record(Translations.ActionExtra("main_flow_toolbar"))
                appStore.dispatch(SnackbarDismissed)
                dependencies.navController.navigateSafe(
                    resId = R.id.browserFragment,
                    directions = BrowserFragmentDirections.actionBrowserFragmentToTranslationsDialogFragment(),
                )
            }

            is RefreshClicked -> {
                val tabId = browserStore.state.selectedTabId
                if (action.bypassCache) {
                    sessionUseCases.reload.invoke(
                        tabId,
                        flags = LoadUrlFlags.select(
                            LoadUrlFlags.BYPASS_CACHE,
                        ),
                    )
                } else {
                    sessionUseCases.reload(tabId)
                }
            }
            is StopRefreshClicked -> {
                val tabId = browserStore.state.selectedTabId
                sessionUseCases.stopLoading(tabId)
            }
            else -> next(action)
        }
    }

    private fun onSiteInfoClicked() {
        val tab = browserStore.state.selectedTab ?: return

        dependencies.lifecycleOwner.lifecycleScope.launch(Dispatchers.IO) {
            val sitePermissions: SitePermissions? = tab.content.url.getOrigin()?.let { origin ->
                permissionsStorage.findSitePermissionsBy(origin, private = tab.content.private)
            }

            dependencies.lifecycleOwner.lifecycleScope.launch(Dispatchers.Main) {
                trackingProtectionUseCases.containsException(tab.id) { hasTrackingProtectionException ->
                    dependencies.lifecycleOwner.lifecycleScope.launch {
                        val cookieBannerUIMode = cookieBannersStorage.getCookieBannerUIMode(
                            tab = tab,
                            isFeatureEnabledInPrivateMode = settings.shouldUseCookieBannerPrivateMode,
                            publicSuffixList = publicSuffixList,
                        )

                        val isTrackingProtectionEnabled =
                            tab.trackingProtection.enabled && !hasTrackingProtectionException
                        val directions = if (settings.enableUnifiedTrustPanel) {
                            BrowserFragmentDirections.actionBrowserFragmentToTrustPanelFragment(
                                sessionId = tab.id,
                                url = tab.content.url,
                                title = tab.content.title,
                                isSecured = tab.content.securityInfo.secure,
                                sitePermissions = sitePermissions,
                                certificateName = tab.content.securityInfo.issuer,
                                permissionHighlights = tab.content.permissionHighlights,
                                isTrackingProtectionEnabled = isTrackingProtectionEnabled,
                                cookieBannerUIMode = cookieBannerUIMode,
                            )
                        } else {
                            BrowserFragmentDirections.actionBrowserFragmentToQuickSettingsSheetDialogFragment(
                                sessionId = tab.id,
                                url = tab.content.url,
                                title = tab.content.title,
                                isLocalPdf = tab.content.url.isContentUrl(),
                                isSecured = tab.content.securityInfo.secure,
                                sitePermissions = sitePermissions,
                                gravity = settings.toolbarPosition.androidGravity,
                                certificateName = tab.content.securityInfo.issuer,
                                permissionHighlights = tab.content.permissionHighlights,
                                isTrackingProtectionEnabled = isTrackingProtectionEnabled,
                                cookieBannerUIMode = cookieBannerUIMode,
                            )
                        }
                        dependencies.navController.nav(
                            R.id.browserFragment,
                            directions,
                        )
                    }
                }
            }
        }
    }

    private fun getCurrentNumberOfOpenedTabs() = when (dependencies.browsingModeManager.mode) {
        Normal -> browserStore.state.normalTabs.size
        Private -> browserStore.state.privateTabs.size
    }

    private fun updateStartBrowserActions() = store?.dispatch(
        BrowserActionsStartUpdated(
            buildStartBrowserActions(),
        ),
    )

    private fun updateStartPageActions() = store?.dispatch(
        BrowserDisplayToolbarAction.PageActionsStartUpdated(
            buildStartPageActions(),
        ),
    )

    private fun updateEndBrowserActions() = store?.dispatch(
        BrowserActionsEndUpdated(
            buildEndBrowserActions(),
        ),
    )

    private fun buildStartPageActions(): List<Action> {
        return listOf(
            ToolbarActionConfig(ToolbarAction.SiteInfo),
        ).filter { config ->
            config.isVisible()
        }.map { config ->
            buildAction(config.action)
        }
    }

    private fun updateEndPageActions() = store?.dispatch(
        PageActionsEndUpdated(
            buildEndPageActions(),
        ),
    )

    private fun buildStartBrowserActions(): List<Action> {
        return listOf(
            ToolbarActionConfig(ToolbarAction.Back) { dependencies.context.isLargeWindow() },
            ToolbarActionConfig(ToolbarAction.Forward) { dependencies.context.isLargeWindow() },
            ToolbarActionConfig(ToolbarAction.RefreshOrStop) { dependencies.context.isLargeWindow() },
        ).filter { config ->
            config.isVisible()
        }.map { config ->
            buildAction(config.action)
        }
    }

    private fun buildEndPageActions(): List<Action> {
        return listOf(
            ToolbarActionConfig(ToolbarAction.ReaderMode) {
                browserScreenStore.state.readerModeStatus.isAvailable
            },
            ToolbarActionConfig(ToolbarAction.Translate) {
                browserScreenStore.state.pageTranslationStatus.isTranslationPossible
            },
        ).filter { config ->
            config.isVisible()
        }.map { config ->
            buildAction(config.action)
        }
    }

    private fun buildEndBrowserActions(): List<Action> {
        return listOf(
            ToolbarActionConfig(ToolbarAction.NewTab) { !dependencies.context.isTabStripEnabled() },
            ToolbarActionConfig(ToolbarAction.TabCounter) { !dependencies.context.isTabStripEnabled() },
            ToolbarActionConfig(ToolbarAction.Menu),
        ).filter { config ->
            config.isVisible()
        }.map { config ->
            buildAction(config.action)
        }
    }

    private fun buildTabCounterMenu() = BrowserToolbarMenu {
        listOf(
            BrowserToolbarMenuButton(
                icon = DrawableResIcon(iconsR.drawable.mozac_ic_plus_24),
                text = StringResText(R.string.mozac_browser_menu_new_tab),
                contentDescription = StringResContentDescription(R.string.mozac_browser_menu_new_tab),
                onClick = AddNewTab,
            ),

            BrowserToolbarMenuButton(
                icon = DrawableResIcon(iconsR.drawable.mozac_ic_private_mode_24),
                text = StringResText(R.string.mozac_browser_menu_new_private_tab),
                contentDescription = StringResContentDescription(R.string.mozac_browser_menu_new_private_tab),
                onClick = AddNewPrivateTab,
            ),

            BrowserToolbarMenuDivider,

            BrowserToolbarMenuButton(
                icon = DrawableResIcon(iconsR.drawable.mozac_ic_cross_24),
                text = StringResText(R.string.mozac_close_tab),
                contentDescription = StringResContentDescription(R.string.mozac_close_tab),
                onClick = CloseCurrentTab,
            ),
        )
    }

    private fun buildProgressBar(progress: Int = 0) = ProgressBarConfig(
        progress = progress,
        gravity = when (settings.shouldUseBottomToolbar) {
            true -> ProgressBarGravity.Top
            false -> ProgressBarGravity.Bottom
        },
    )

    private fun openNewTab(browsingMode: BrowsingMode) {
        dependencies.browsingModeManager.mode = browsingMode
        dependencies.navController.navigate(
            BrowserFragmentDirections.actionGlobalHome(focusOnAddressBar = true),
        )
    }

    private fun observeProgressBarUpdates() {
        observeWhileActive(browserStore) {
            distinctUntilChangedBy { it.selectedTab?.content?.progress }
            .collect {
                store?.dispatch(
                    UpdateProgressBarConfig(
                        buildProgressBar(it.selectedTab?.content?.progress ?: 0),
                    ),
                )
            }
        }
    }

    private fun updateToolbarActionsBasedOnOrientation() {
        observeWhileActive(appStore) {
            distinctUntilChangedBy { it.orientation }
            .collect {
                updateEndBrowserActions()
            }
        }
    }

    private fun observeTabsCountUpdates() {
        observeWhileActive(browserStore) {
            distinctUntilChangedBy { it.tabs.size }
            .collect {
                updateEndBrowserActions()
            }
        }
    }

    private fun observePageOriginUpdates() {
        observeWhileActive(browserStore) {
            distinctUntilChangedBy { it.selectedTab?.content?.url }
            .collect {
                updateCurrentPageOrigin()
            }
        }
    }

    private fun updateCurrentPageOrigin() = MainScope().launch {
        val url = browserStore.state.selectedTab?.content?.url
        val displayUrl = url?.let { originalUrl ->
            if (originalUrl == ABOUT_HOME) {
                // Default to showing the toolbar hint when the URL is ABOUT_HOME.
                ""
            } else {
                URLStringUtils.toDisplayUrl(originalUrl).toString()
            }
        }
        val registrableDomainIndexRange = when (displayUrl != null && displayUrl.isNotEmpty()) {
            true -> getRegistrableDomainOrHostIndexRange(url, displayUrl, publicSuffixList)
            false -> null
        }

        store?.dispatch(
            BrowserDisplayToolbarAction.PageOriginUpdated(
                PageOrigin(
                    hint = R.string.search_hint,
                    title = null,
                    url = displayUrl,
                    registrableDomainIndexRange = registrableDomainIndexRange,
                    contextualMenuOptions = ContextualMenuOption.entries,
                    onClick = OriginClicked,
                ),
            ),
        )
    }

    private fun observePageSecurityUpdates() {
        observeWhileActive(browserStore) {
            distinctUntilChangedBy { it.selectedTab?.content?.securityInfo }
                .collect {
                    updateStartPageActions()
                }
        }
    }

    private fun observeAcceptingCancellingPrivateDownloads() {
        observeWhileActive(browserScreenStore) {
            distinctUntilChangedBy { it.cancelPrivateDownloadsAccepted }
            .collect {
                if (it.cancelPrivateDownloadsAccepted) {
                    store?.dispatch(CloseCurrentTab)
                }
            }
        }
    }

    private fun observeReaderModeUpdates() {
        observeWhileActive(browserScreenStore) {
            distinctUntilChangedBy { it.readerModeStatus }
                .collect {
                    updateEndPageActions()
                }
        }
    }

    private fun observePageTranslationsUpdates() {
        observeWhileActive(browserScreenStore) {
            distinctUntilChangedBy { it.pageTranslationStatus }
            .collect {
                updateEndPageActions()
            }
        }
    }

    private inline fun <S : State, A : MVIAction> observeWhileActive(
        store: Store<S, A>,
        crossinline observe: suspend (Flow<S>.() -> Unit),
    ) {
        with(dependencies.lifecycleOwner) {
            lifecycleScope.launch {
                repeatOnLifecycle(RESUMED) {
                    store.flow().observe()
                }
            }
        }
    }

    private fun observePageNavigationStatus() {
        with(dependencies.lifecycleOwner) {
            this.lifecycleScope.launch {
                repeatOnLifecycle(RESUMED) {
                    browserStore.flow()
                        .distinctUntilChangedBy {
                            arrayOf(
                                it.selectedTab?.content?.canGoBack,
                                it.selectedTab?.content?.canGoForward,
                            )
                        }
                        .collect {
                            updateStartBrowserActions()
                        }
                }
            }
        }
    }

    private fun observePageRefreshUpdates() {
        with(dependencies.lifecycleOwner) {
            this.lifecycleScope.launch {
                repeatOnLifecycle(RESUMED) {
                    browserStore.flow()
                        .distinctUntilChangedBy {
                            it.selectedTab?.content?.loading == true
                        }
                        .collect {
                            updateStartBrowserActions()
                        }
                }
            }
        }
    }

    /**
     * Lifecycle dependencies for the [BrowserToolbarMiddleware].
     *
     * @property context [Context] used for various system interactions.
     * @property lifecycleOwner [LifecycleOwner] depending on which lifecycle related operations will be scheduled.
     * @property navController [NavController] to use for navigating to other in-app destinations.
     * @property browsingModeManager [BrowsingModeManager] for querying the current browsing mode.
     * @property browserAnimator Helper for animating the browser content when navigating to other screens.
     * @property thumbnailsFeature [BrowserThumbnails] for requesting screenshots of the current tab.
     * @property readerModeController [ReaderModeController] for showing or hiding the reader view UX.
     */
    data class LifecycleDependencies(
        val context: Context,
        val lifecycleOwner: LifecycleOwner,
        val navController: NavController,
        val browsingModeManager: BrowsingModeManager,
        val browserAnimator: BrowserAnimator,
        val thumbnailsFeature: BrowserThumbnails?,
        val readerModeController: ReaderModeController,
    )

    @VisibleForTesting
    internal enum class ToolbarAction {
        NewTab,
        Back,
        Forward,
        RefreshOrStop,
        Menu,
        ReaderMode,
        Translate,
        TabCounter,
        SiteInfo,
    }

    private data class ToolbarActionConfig(
        val action: ToolbarAction,
        val isVisible: () -> Boolean = { true },
    )

    @Suppress("LongMethod")
    @VisibleForTesting
    internal fun buildAction(
        toolbarAction: ToolbarAction,
    ): Action = when (toolbarAction) {
        ToolbarAction.NewTab -> ActionButtonRes(
            drawableResId = R.drawable.mozac_ic_plus_24,
            contentDescription = if (dependencies.browsingModeManager.mode == Private) {
                R.string.home_screen_shortcut_open_new_private_tab_2
            } else {
                R.string.home_screen_shortcut_open_new_tab_2
            },
            onClick = if (dependencies.browsingModeManager.mode == Private) {
                AddNewPrivateTab
            } else {
                AddNewTab
            },
        )

        ToolbarAction.Back -> ActionButtonRes(
            drawableResId = R.drawable.mozac_ic_back_24,
            contentDescription = R.string.browser_menu_back,
            state = if (browserStore.state.selectedTab?.content?.canGoBack == true) {
                ActionButton.State.DEFAULT
            } else {
                ActionButton.State.DISABLED
            },
            onClick = NavigateBackClicked,
            onLongClick = NavigateSessionLongClicked,
        )

        ToolbarAction.Forward -> ActionButtonRes(
            drawableResId = R.drawable.mozac_ic_forward_24,
            contentDescription = R.string.browser_menu_forward,
            state = if (browserStore.state.selectedTab?.content?.canGoForward == true) {
                ActionButton.State.DEFAULT
            } else {
                ActionButton.State.DISABLED
            },
            onClick = NavigateForwardClicked,
            onLongClick = NavigateSessionLongClicked,
        )

        ToolbarAction.RefreshOrStop -> {
            if (browserStore.state.selectedTab?.content?.loading != true) {
                ActionButtonRes(
                    drawableResId = R.drawable.mozac_ic_arrow_clockwise_24,
                    contentDescription = R.string.browser_menu_refresh,
                    onClick = RefreshClicked(bypassCache = false),
                    onLongClick = RefreshClicked(bypassCache = true),
                )
            } else {
                ActionButtonRes(
                    drawableResId = R.drawable.mozac_ic_cross_24,
                    contentDescription = R.string.browser_menu_stop,
                    onClick = StopRefreshClicked,
                )
            }
        }

        ToolbarAction.Menu -> ActionButtonRes(
            drawableResId = R.drawable.mozac_ic_ellipsis_vertical_24,
            contentDescription = R.string.content_description_menu,
            onClick = MenuClicked,
        )

        ToolbarAction.ReaderMode -> ActionButtonRes(
            drawableResId = R.drawable.ic_readermode,
            contentDescription = if (browserScreenStore.state.readerModeStatus.isActive) {
                R.string.browser_menu_read_close
            } else {
                R.string.browser_menu_read
            },
            state = if (browserScreenStore.state.readerModeStatus.isActive) {
                ActionButton.State.ACTIVE
            } else {
                ActionButton.State.DEFAULT
            },
            onClick = ReaderModeClicked(browserScreenStore.state.readerModeStatus.isActive),
        )

        ToolbarAction.Translate -> ActionButtonRes(
            drawableResId = R.drawable.mozac_ic_translate_24,
            contentDescription = R.string.browser_toolbar_translate,
            state = if (browserScreenStore.state.pageTranslationStatus.isTranslated) {
                ActionButton.State.ACTIVE
            } else {
                ActionButton.State.DEFAULT
            },
            onClick = TranslateClicked,
        )

        ToolbarAction.TabCounter -> {
            val tabsCount = getCurrentNumberOfOpenedTabs()
            val isPrivateMode = dependencies.browsingModeManager.mode.isPrivate
            val context = dependencies.context

            val tabCounterDescription = if (isPrivateMode) {
                context.getString(
                    R.string.mozac_tab_counter_private,
                    tabsCount.toString(),
                )
            } else {
                context.getString(
                    R.string.mozac_tab_counter_open_tab_tray,
                    tabsCount.toString(),
                )
            }

            TabCounterAction(
                count = tabsCount,
                contentDescription = tabCounterDescription,
                showPrivacyMask = isPrivateMode,
                onClick = TabCounterClicked,
                onLongClick = buildTabCounterMenu(),
            )
        }

        ToolbarAction.SiteInfo -> {
            if (browserStore.state.selectedTab?.content?.url?.isContentUrl() == true) {
                ActionButtonRes(
                    drawableResId = R.drawable.mozac_ic_page_portrait_24,
                    contentDescription = R.string.mozac_browser_toolbar_content_description_site_info,
                    onClick = StartPageActions.SiteInfoClicked,
                )
            } else if (browserStore.state.selectedTab?.content?.securityInfo?.secure == true) {
                ActionButtonRes(
                    drawableResId = R.drawable.mozac_ic_shield_checkmark_24,
                    contentDescription = R.string.mozac_browser_toolbar_content_description_site_info,
                    onClick = StartPageActions.SiteInfoClicked,
                )
            } else {
                ActionButtonRes(
                    drawableResId = R.drawable.mozac_ic_shield_slash_24,
                    contentDescription = R.string.mozac_browser_toolbar_content_description_site_info,
                    onClick = StartPageActions.SiteInfoClicked,
                )
            }
        }
    }

    /**
     * Static functionalities of the [BrowserToolbarMiddleware].
     */
    companion object {
        /**
         * [ViewModelProvider.Factory] for creating a [BrowserToolbarMiddleware].
         *
         * @param appStore [AppStore] allowing to integrate with other features of the applications.
         * @param browserScreenStore [BrowserScreenStore] used for integration with other
         * browser screen functionalities.
         * @param permissionsStorage [SitePermissionsStorage] to find currently selected tab site permissions.
         * @param cookieBannersStorage [CookieBannersStorage] to get the current status of cookie banner ui mode.
         * @param browserStore [BrowserStore] to sync from.
         * @param trackingProtectionUseCases [TrackingProtectionUseCases] allowing to query
         * tracking protection data of the current tab.
         * @param useCases [UseCases] helping this integrate with other features of the applications.
         * @param clipboard [ClipboardHandler] to use for reading from device's clipboard.
         * @param publicSuffixList [PublicSuffixList] used to obtain the base domain of the current site.
         * @param settings [Settings] for accessing user preferences.
         */
        @Suppress("LongParameterList")
        fun viewModelFactory(
            appStore: AppStore,
            browserScreenStore: BrowserScreenStore,
            permissionsStorage: SitePermissionsStorage,
            cookieBannersStorage: CookieBannersStorage,
            browserStore: BrowserStore,
            trackingProtectionUseCases: TrackingProtectionUseCases,
            useCases: UseCases,
            clipboard: ClipboardHandler,
            publicSuffixList: PublicSuffixList,
            settings: Settings,
        ): ViewModelProvider.Factory = object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T {
                if (modelClass.isAssignableFrom(BrowserToolbarMiddleware::class.java)) {
                    return BrowserToolbarMiddleware(
                        appStore = appStore,
                        browserScreenStore = browserScreenStore,
                        permissionsStorage = permissionsStorage,
                        cookieBannersStorage = cookieBannersStorage,
                        browserStore = browserStore,
                        trackingProtectionUseCases = trackingProtectionUseCases,
                        useCases = useCases,
                        clipboard = clipboard,
                        publicSuffixList = publicSuffixList,
                        settings = settings,
                    ) as T
                }
                throw IllegalArgumentException("Unknown ViewModel class")
            }
        }
    }
}
