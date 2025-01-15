/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Intent
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AlertDialog
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.navigation.NavController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.ext.getUrl
import mozilla.components.browser.state.selector.findCustomTabOrSelectedTab
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.feature.session.SessionFeature
import mozilla.components.feature.top.sites.DefaultTopSitesStorage
import mozilla.components.feature.top.sites.PinnedSiteStorage
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.utils.ManufacturerCodes
import mozilla.components.ui.widgets.withCenterAlignedButtons
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.GleanMetrics.AppMenu
import org.mozilla.fenix.GleanMetrics.Collections
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.GleanMetrics.NavigationBar
import org.mozilla.fenix.GleanMetrics.ReaderMode
import org.mozilla.fenix.GleanMetrics.Translations
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserAnimator
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.browser.readermode.ReaderModeController
import org.mozilla.fenix.collections.SaveCollectionStep
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.accounts.AccountState
import org.mozilla.fenix.components.accounts.FenixFxAEntryPoint
import org.mozilla.fenix.components.appstate.AppAction.ShortcutAction
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.navigateSafe
import org.mozilla.fenix.ext.openSetDefaultBrowserOption
import org.mozilla.fenix.settings.deletebrowsingdata.deleteAndQuit
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.webcompat.WEB_COMPAT_REPORTER_URL

/**
 * An interface that handles events from the BrowserToolbar menu, triggered by the Interactor
 */
interface BrowserToolbarMenuController {
    fun handleToolbarItemInteraction(item: ToolbarMenu.Item)
}

@Suppress("LargeClass", "ForbiddenComment", "LongParameterList")
class DefaultBrowserToolbarMenuController(
    private val fragment: Fragment,
    private val store: BrowserStore,
    private val appStore: AppStore,
    private val activity: HomeActivity,
    private val navController: NavController,
    private val settings: Settings,
    private val readerModeController: ReaderModeController,
    private val sessionFeature: ViewBoundFeatureWrapper<SessionFeature>,
    private val findInPageLauncher: () -> Unit,
    private val browserAnimator: BrowserAnimator,
    private val customTabSessionId: String?,
    private val openInFenixIntent: Intent,
    private val bookmarkTapped: (String, String) -> Unit,
    private val scope: CoroutineScope,
    private val tabCollectionStorage: TabCollectionStorage,
    private val topSitesStorage: DefaultTopSitesStorage,
    private val pinnedSiteStorage: PinnedSiteStorage,
) : BrowserToolbarMenuController {

    private val currentSession
        get() = store.state.findCustomTabOrSelectedTab(customTabSessionId)

    // We hold onto a reference of the inner scope so that we can override this with the
    // TestCoroutineScope to ensure sequential execution. If we didn't have this, our tests
    // would fail intermittently due to the async nature of coroutine scheduling.
    @VisibleForTesting
    internal var ioScope: CoroutineScope = CoroutineScope(Dispatchers.IO)

    @Suppress("ComplexMethod", "LongMethod")
    override fun handleToolbarItemInteraction(item: ToolbarMenu.Item) {
        val sessionUseCases = activity.components.useCases.sessionUseCases
        val customTabUseCases = activity.components.useCases.customTabsUseCases
        val tabsUseCases = activity.components.useCases.tabsUseCases
        trackToolbarItemInteraction(item)

        when (item) {
            // TODO: These can be removed for https://github.com/mozilla-mobile/fenix/issues/17870
            // todo === Start ===
            is ToolbarMenu.Item.InstallPwaToHomeScreen -> {
                settings.installPwaOpened = true
                MainScope().launch {
                    with(activity.components.useCases.webAppUseCases) {
                        if (isInstallable()) {
                            addToHomescreen()
                        } else {
                            val directions =
                                BrowserFragmentDirections.actionBrowserFragmentToCreateShortcutFragment()
                            navController.navigateSafe(R.id.browserFragment, directions)
                        }
                    }
                }
            }
            is ToolbarMenu.Item.OpenInFenix -> {
                customTabSessionId?.let {
                    // Stop the SessionFeature from updating the EngineView and let it release the session
                    // from the EngineView so that it can immediately be rendered by a different view once
                    // we switch to the actual browser.
                    sessionFeature.get()?.release()

                    // Turn this Session into a regular tab and then select it
                    customTabUseCases.migrate(customTabSessionId, select = true)

                    // Switch to the actual browser which should now display our new selected session
                    activity.startActivity(
                        openInFenixIntent.apply {
                            // We never want to launch the browser in the same task as the external app
                            // activity. So we force a new task here. IntentReceiverActivity will do the
                            // right thing and take care of routing to an already existing browser and avoid
                            // cloning a new one.
                            flags = flags or Intent.FLAG_ACTIVITY_NEW_TASK
                        },
                    )

                    // Close this activity (and the task) since it is no longer displaying any session
                    activity.finishAndRemoveTask()
                }
            }
            // todo === End ===
            is ToolbarMenu.Item.OpenInApp -> {
                settings.openInAppOpened = true

                val appLinksUseCases = activity.components.useCases.appLinksUseCases
                val getRedirect = appLinksUseCases.appLinkRedirect
                currentSession?.let {
                    val redirect = getRedirect.invoke(it.content.url)
                    redirect.appIntent?.flags = Intent.FLAG_ACTIVITY_NEW_TASK
                    appLinksUseCases.openAppLink.invoke(redirect.appIntent)
                }
            }
            is ToolbarMenu.Item.Quit -> {
                deleteAndQuit(activity, activity.lifecycleScope)
            }
            is ToolbarMenu.Item.CustomizeReaderView -> {
                readerModeController.showControls()
                ReaderMode.appearance.record(NoExtras())
            }
            is ToolbarMenu.Item.Back -> {
                if (item.viewHistory) {
                    navController.navigate(
                        BrowserFragmentDirections.actionGlobalTabHistoryDialogFragment(
                            activeSessionId = customTabSessionId,
                        ),
                    )
                } else {
                    currentSession?.let {
                        sessionUseCases.goBack.invoke(it.id)
                    }
                }
            }
            is ToolbarMenu.Item.Forward -> {
                if (item.viewHistory) {
                    navController.navigate(
                        BrowserFragmentDirections.actionGlobalTabHistoryDialogFragment(
                            activeSessionId = customTabSessionId,
                        ),
                    )
                } else {
                    currentSession?.let {
                        sessionUseCases.goForward.invoke(it.id)
                    }
                }
            }
            is ToolbarMenu.Item.Reload -> {
                val flags = if (item.bypassCache) {
                    LoadUrlFlags.select(LoadUrlFlags.BYPASS_CACHE)
                } else {
                    LoadUrlFlags.none()
                }

                currentSession?.let {
                    sessionUseCases.reload.invoke(it.id, flags = flags)
                }
            }
            is ToolbarMenu.Item.Stop -> {
                currentSession?.let {
                    sessionUseCases.stopLoading.invoke(it.id)
                }
            }
            is ToolbarMenu.Item.Share -> {
                val sessionId = currentSession?.id
                val url = sessionId?.let {
                    store.state.findTab(it)?.getUrl()
                } ?: currentSession?.content?.url
                val directions = NavGraphDirections.actionGlobalShareFragment(
                    sessionId = sessionId,
                    data = arrayOf(
                        ShareData(
                            url = url,
                            title = currentSession?.content?.title,
                        ),
                    ),
                    showPage = true,
                )
                navController.navigate(directions)
            }
            is ToolbarMenu.Item.Settings -> browserAnimator.captureEngineViewAndDrawStatically {
                val directions = BrowserFragmentDirections.actionBrowserFragmentToSettingsFragment()
                navController.nav(R.id.browserFragment, directions)
            }
            is ToolbarMenu.Item.SyncAccount -> {
                val directions = when (item.accountState) {
                    AccountState.AUTHENTICATED ->
                        BrowserFragmentDirections.actionGlobalAccountSettingsFragment()
                    AccountState.NEEDS_REAUTHENTICATION ->
                        BrowserFragmentDirections.actionGlobalAccountProblemFragment(
                            entrypoint = FenixFxAEntryPoint.BrowserToolbar,
                        )
                    AccountState.NO_ACCOUNT ->
                        BrowserFragmentDirections.actionGlobalTurnOnSync(entrypoint = FenixFxAEntryPoint.BrowserToolbar)
                }
                browserAnimator.captureEngineViewAndDrawStatically {
                    navController.nav(
                        R.id.browserFragment,
                        directions,
                    )
                }
            }
            is ToolbarMenu.Item.RequestDesktop -> {
                currentSession?.let {
                    sessionUseCases.requestDesktopSite.invoke(
                        item.isChecked,
                        it.id,
                    )
                }
            }
            is ToolbarMenu.Item.OpenInRegularTab -> {
                currentSession?.id?.let { sessionId ->
                    store.state.findTab(sessionId)?.getUrl()?.let { url ->
                        tabsUseCases.migratePrivateTabUseCase.invoke(sessionId, url)
                    }
                }
            }
            is ToolbarMenu.Item.AddToTopSites -> {
                scope.launch {
                    val numPinnedSites = topSitesStorage.cachedTopSites
                        .filter { it is TopSite.Default || it is TopSite.Pinned }.size

                    if (numPinnedSites >= settings.topSitesMaxLimit) {
                        AlertDialog.Builder(fragment.requireContext()).apply {
                            setTitle(R.string.shortcut_max_limit_title)
                            setMessage(R.string.shortcut_max_limit_content)
                            setPositiveButton(R.string.top_sites_max_limit_confirmation_button) { dialog, _ ->
                                dialog.dismiss()
                            }
                            create().withCenterAlignedButtons()
                        }.show()
                    } else {
                        ioScope.launch {
                            currentSession?.let {
                                with(activity.components.useCases.topSitesUseCase) {
                                    addPinnedSites(it.content.title, it.content.url)
                                }
                            }
                        }.join()

                        appStore.dispatch(ShortcutAction.ShortcutAdded)
                    }
                }
            }
            is ToolbarMenu.Item.AddToHomeScreen -> {
                settings.installPwaOpened = true
                MainScope().launch {
                    with(activity.components.useCases.webAppUseCases) {
                        if (isInstallable()) {
                            addToHomescreen()
                        } else {
                            if (ManufacturerCodes.isXiaomi) {
                                val directions =
                                    BrowserFragmentDirections.actionBrowserFragmentToCreateXiaomiShortcutFragment()
                                navController.navigateSafe(R.id.browserFragment, directions)
                            } else {
                                val directions =
                                    BrowserFragmentDirections.actionBrowserFragmentToCreateShortcutFragment()
                                navController.navigateSafe(R.id.browserFragment, directions)
                            }
                        }
                    }
                }
            }
            is ToolbarMenu.Item.FindInPage -> {
                findInPageLauncher()
            }
            is ToolbarMenu.Item.AddonsManager -> browserAnimator.captureEngineViewAndDrawStatically {
                navController.nav(
                    R.id.browserFragment,
                    BrowserFragmentDirections.actionGlobalAddonsManagementFragment(),
                )
            }
            is ToolbarMenu.Item.SaveToCollection -> {
                Collections.saveButton.record(
                    Collections.SaveButtonExtra(
                        TELEMETRY_BROWSER_IDENTIFIER,
                    ),
                )

                currentSession?.let { currentSession ->
                    val directions =
                        BrowserFragmentDirections.actionGlobalCollectionCreationFragment(
                            tabIds = arrayOf(currentSession.id),
                            selectedTabIds = arrayOf(currentSession.id),
                            saveCollectionStep = if (tabCollectionStorage.cachedTabCollections.isEmpty()) {
                                SaveCollectionStep.NameCollection
                            } else {
                                SaveCollectionStep.SelectCollection
                            },
                        )
                    navController.nav(R.id.browserFragment, directions)
                }
            }
            is ToolbarMenu.Item.PrintContent -> {
                store.state.selectedTab?.let {
                    store.dispatch(EngineAction.PrintContentAction(it.id))
                }
            }
            is ToolbarMenu.Item.Bookmark -> {
                store.state.selectedTab?.let {
                    it.getUrl()?.let { url -> bookmarkTapped(url, it.content.title) }
                }
            }
            is ToolbarMenu.Item.Bookmarks -> browserAnimator.captureEngineViewAndDrawStatically {
                navController.nav(
                    R.id.browserFragment,
                    BrowserFragmentDirections.actionGlobalBookmarkFragment(BookmarkRoot.Mobile.id),
                )
            }
            is ToolbarMenu.Item.History -> browserAnimator.captureEngineViewAndDrawStatically {
                navController.nav(
                    R.id.browserFragment,
                    BrowserFragmentDirections.actionGlobalHistoryFragment(),
                )
            }
            is ToolbarMenu.Item.Passwords -> {
                navController.nav(
                    R.id.browserFragment,
                    BrowserFragmentDirections.actionLoginsListFragment(),
                )
            }
            is ToolbarMenu.Item.Downloads -> browserAnimator.captureEngineViewAndDrawStatically {
                navController.nav(
                    R.id.browserFragment,
                    BrowserFragmentDirections.actionGlobalDownloadsFragment(),
                )
            }
            is ToolbarMenu.Item.NewTab -> {
                navController.navigate(
                    BrowserFragmentDirections.actionGlobalHome(focusOnAddressBar = true),
                )
            }
            is ToolbarMenu.Item.SetDefaultBrowser -> {
                activity.openSetDefaultBrowserOption()
            }
            is ToolbarMenu.Item.RemoveFromTopSites -> {
                scope.launch {
                    val removedTopSite: TopSite? =
                        pinnedSiteStorage
                            .getPinnedSites()
                            .find { it.url == currentSession?.content?.url }
                    if (removedTopSite != null) {
                        ioScope.launch {
                            currentSession?.let {
                                with(activity.components.useCases.topSitesUseCase) {
                                    removeTopSites(removedTopSite)
                                }
                            }
                        }.join()
                    }

                    appStore.dispatch(ShortcutAction.ShortcutRemoved)
                }
            }
            is ToolbarMenu.Item.ReportBrokenSite -> {
                currentSession?.content?.url?.let { tabUrl ->
                    if (FeatureFlags.webCompatReporter && settings.isTelemetryEnabled) {
                        navController.navigate(
                            directions = BrowserFragmentDirections
                                .actionBrowserFragmentToWebCompatReporterFragment(
                                    tabUrl = tabUrl,
                                ),
                        )
                    } else {
                        activity.openToBrowserAndLoad(
                            searchTermOrURL = "$WEB_COMPAT_REPORTER_URL$tabUrl",
                            newTab = true,
                            from = BrowserDirection.FromGlobal,
                        )
                    }
                }
            }

            ToolbarMenu.Item.Translate -> {
                Translations.action.record(Translations.ActionExtra("main_flow_browser"))
                val directions =
                    BrowserFragmentDirections.actionBrowserFragmentToTranslationsDialogFragment()
                navController.navigateSafe(R.id.browserFragment, directions)
            }
        }
    }

    @Suppress("ComplexMethod", "LongMethod")
    private fun trackToolbarItemInteraction(item: ToolbarMenu.Item) {
        when (item) {
            is ToolbarMenu.Item.OpenInFenix ->
                if (item.isOnNavBar) {
                    NavigationBar.customOpenInFenixTapped.record(NoExtras())
                } else if (item.isOnToolbar) {
                    Events.browserToolbarAction.record(Events.BrowserToolbarActionExtra("open_in_fenix"))
                } else {
                    Events.browserMenuAction.record(Events.BrowserMenuActionExtra("open_in_fenix"))
                }
            is ToolbarMenu.Item.InstallPwaToHomeScreen ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("add_to_homescreen"))
            is ToolbarMenu.Item.Quit ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("quit"))
            is ToolbarMenu.Item.OpenInApp ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("open_in_app"))
            is ToolbarMenu.Item.CustomizeReaderView ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("reader_mode_appearance"))
            is ToolbarMenu.Item.Back -> {
                when {
                    item.isOnNavBar && item.isCustomTab && item.viewHistory ->
                        NavigationBar.customBackLongTapped.record(NoExtras())
                    item.isOnNavBar && item.isCustomTab && !item.viewHistory ->
                        NavigationBar.customBackTapped.record(NoExtras())
                    item.isOnNavBar && !item.isCustomTab && item.viewHistory ->
                        NavigationBar.browserBackLongTapped.record(NoExtras())
                    item.isOnNavBar && !item.isCustomTab && !item.viewHistory ->
                        NavigationBar.browserBackTapped.record(NoExtras())
                    item.isOnToolbar && item.isCustomTab && item.viewHistory ->
                        Events.browserToolbarAction.record(Events.BrowserToolbarActionExtra("custom_back_long_press"))
                    item.isOnToolbar && item.isCustomTab && !item.viewHistory ->
                        Events.browserToolbarAction.record(Events.BrowserToolbarActionExtra("custom_back"))
                    item.isOnToolbar && !item.isCustomTab && item.viewHistory ->
                        Events.browserToolbarAction.record(Events.BrowserToolbarActionExtra("back_long_press"))
                    item.isOnToolbar && !item.isCustomTab && !item.viewHistory ->
                        Events.browserToolbarAction.record(Events.BrowserToolbarActionExtra("back"))
                    item.viewHistory ->
                        Events.browserMenuAction.record(Events.BrowserMenuActionExtra("back_long_press"))
                    else -> Events.browserMenuAction.record(Events.BrowserMenuActionExtra("back"))
                }
            }
            is ToolbarMenu.Item.Forward ->
                when {
                    item.isOnNavBar && item.isCustomTab && item.viewHistory ->
                        NavigationBar.customBackLongTapped.record(NoExtras())
                    item.isOnNavBar && item.isCustomTab && !item.viewHistory ->
                        NavigationBar.customBackTapped.record(NoExtras())
                    item.isOnNavBar && !item.isCustomTab && item.viewHistory ->
                        NavigationBar.browserBackLongTapped.record(NoExtras())
                    item.isOnNavBar && !item.isCustomTab && !item.viewHistory ->
                        NavigationBar.browserBackTapped.record(NoExtras())
                    item.isOnToolbar && item.isCustomTab && item.viewHistory ->
                        Events.browserToolbarAction.record(
                            Events.BrowserToolbarActionExtra("custom_forward_long_press"),
                        )
                    item.isOnToolbar && item.isCustomTab && !item.viewHistory ->
                        Events.browserToolbarAction.record(Events.BrowserToolbarActionExtra("custom_forward"))
                    item.isOnToolbar && !item.isCustomTab && item.viewHistory ->
                        Events.browserToolbarAction.record(Events.BrowserToolbarActionExtra("forward_long_press"))
                    item.isOnToolbar && !item.isCustomTab && !item.viewHistory ->
                        Events.browserToolbarAction.record(Events.BrowserToolbarActionExtra("forward"))
                    item.viewHistory ->
                        Events.browserMenuAction.record(Events.BrowserMenuActionExtra("forward_long_press"))
                    else -> Events.browserMenuAction.record(Events.BrowserMenuActionExtra("forward"))
                }

            is ToolbarMenu.Item.Reload ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("reload"))
            is ToolbarMenu.Item.Stop ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("stop"))
            is ToolbarMenu.Item.Share ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("share"))
            is ToolbarMenu.Item.Settings ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("settings"))
            is ToolbarMenu.Item.RequestDesktop ->
                if (item.isChecked) {
                    Events.browserMenuAction.record(Events.BrowserMenuActionExtra("desktop_view_on"))
                } else {
                    Events.browserMenuAction.record(Events.BrowserMenuActionExtra("desktop_view_off"))
                }
            is ToolbarMenu.Item.OpenInRegularTab ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("open_in_regular_tab"))
            is ToolbarMenu.Item.FindInPage ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("find_in_page"))
            is ToolbarMenu.Item.SaveToCollection ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("save_to_collection"))
            is ToolbarMenu.Item.AddToTopSites ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("add_to_top_sites"))
            is ToolbarMenu.Item.PrintContent ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("print_content"))
            is ToolbarMenu.Item.AddToHomeScreen ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("add_to_homescreen"))
            is ToolbarMenu.Item.SyncAccount -> {
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("sync_account"))
                AppMenu.signIntoSync.add()
            }
            is ToolbarMenu.Item.Bookmark ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("bookmark"))
            is ToolbarMenu.Item.AddonsManager ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("addons_manager"))
            is ToolbarMenu.Item.Bookmarks ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("bookmarks"))
            is ToolbarMenu.Item.History ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("history"))
            is ToolbarMenu.Item.Passwords ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("passwords"))
            is ToolbarMenu.Item.Downloads ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("downloads"))
            is ToolbarMenu.Item.NewTab ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("new_tab"))
            is ToolbarMenu.Item.SetDefaultBrowser ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("set_default_browser"))
            is ToolbarMenu.Item.RemoveFromTopSites ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("remove_from_top_sites"))
            is ToolbarMenu.Item.ReportBrokenSite ->
                Events.browserMenuAction.record(
                    Events.BrowserMenuActionExtra(
                        item = "report_broken_site",
                    ),
                )

            ToolbarMenu.Item.Translate -> Events.browserMenuAction.record(
                Events.BrowserMenuActionExtra(
                    "translate",
                ),
            )
        }
    }

    companion object {
        internal const val TELEMETRY_BROWSER_IDENTIFIER = "browserMenu"
    }
}
