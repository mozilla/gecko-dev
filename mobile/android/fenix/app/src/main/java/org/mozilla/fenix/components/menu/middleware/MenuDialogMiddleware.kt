/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.middleware

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.browser.state.ext.getUrl
import mozilla.components.concept.engine.webextension.InstallationMethod
import mozilla.components.concept.storage.BookmarksStorage
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.AddonManager
import mozilla.components.feature.addons.AddonManagerException
import mozilla.components.feature.top.sites.PinnedSiteStorage
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.feature.top.sites.TopSitesUseCases
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Store
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppAction.BookmarkAction
import org.mozilla.fenix.components.bookmarks.BookmarksUseCase
import org.mozilla.fenix.components.menu.store.BookmarkState
import org.mozilla.fenix.components.menu.store.MenuAction
import org.mozilla.fenix.components.menu.store.MenuState

/**
 * [Middleware] implementation for handling [MenuAction] and managing the [MenuState] for the menu
 * dialog.
 *
 * @param appStore The [AppStore] used to dispatch actions to update the global state.
 * @param addonManager An instance of the [AddonManager] used to provide access to [Addon]s.
 * @param bookmarksStorage An instance of the [BookmarksStorage] used
 * to query matching bookmarks.
 * @param pinnedSiteStorage An instance of the [PinnedSiteStorage] used
 * to query matching pinned shortcuts.
 * @param addBookmarkUseCase The [BookmarksUseCase.AddBookmarksUseCase] for adding the
 * selected tab as a bookmark.
 * @param addPinnedSiteUseCase The [TopSitesUseCases.AddPinnedSiteUseCase] for adding the
 * selected tab as a pinned shortcut.
 * @param removePinnedSitesUseCase The [TopSitesUseCases.RemoveTopSiteUseCase] for removing the
 * selected tab from pinned shortcuts.
 * @param topSitesMaxLimit The maximum number of top sites the user can have.
 * @param onDeleteAndQuit Callback invoked to delete browsing data and quit the browser.
 * @param onDismiss Callback invoked to dismiss the menu dialog.
 * @param scope [CoroutineScope] used to launch coroutines.
 */
@Suppress("LongParameterList")
class MenuDialogMiddleware(
    private val appStore: AppStore,
    private val addonManager: AddonManager,
    private val bookmarksStorage: BookmarksStorage,
    private val pinnedSiteStorage: PinnedSiteStorage,
    private val addBookmarkUseCase: BookmarksUseCase.AddBookmarksUseCase,
    private val addPinnedSiteUseCase: TopSitesUseCases.AddPinnedSiteUseCase,
    private val removePinnedSitesUseCase: TopSitesUseCases.RemoveTopSiteUseCase,
    private val topSitesMaxLimit: Int,
    private val onDeleteAndQuit: () -> Unit,
    private val onDismiss: suspend () -> Unit,
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.IO),
) : Middleware<MenuState, MenuAction> {

    private val logger = Logger("MenuDialogMiddleware")

    override fun invoke(
        context: MiddlewareContext<MenuState, MenuAction>,
        next: (MenuAction) -> Unit,
        action: MenuAction,
    ) {
        when (action) {
            is MenuAction.InitAction -> initialize(context.store)
            is MenuAction.AddBookmark -> addBookmark(context.store)
            is MenuAction.AddShortcut -> addShortcut(context.store)
            is MenuAction.RemoveShortcut -> removeShortcut(context.store)
            is MenuAction.DeleteBrowsingDataAndQuit -> deleteBrowsingDataAndQuit()
            is MenuAction.InstallAddon -> installAddon(action.addon)
            else -> Unit
        }

        next(action)
    }

    private fun initialize(
        store: Store<MenuState, MenuAction>,
    ) = scope.launch {
        setupBookmarkState(store)
        setupPinnedState(store)
        setupExtensionState(store)
    }

    private suspend fun setupBookmarkState(
        store: Store<MenuState, MenuAction>,
    ) {
        val url = store.state.browserMenuState?.selectedTab?.content?.url ?: return
        val bookmark = bookmarksStorage.getBookmarksWithUrl(url)
            .firstOrNull { it.url == url } ?: return

        store.dispatch(
            MenuAction.UpdateBookmarkState(
                bookmarkState = BookmarkState(
                    guid = bookmark.guid,
                    isBookmarked = true,
                ),
            ),
        )
    }

    private suspend fun setupPinnedState(
        store: Store<MenuState, MenuAction>,
    ) {
        val url = store.state.browserMenuState?.selectedTab?.content?.url ?: return
        pinnedSiteStorage.getPinnedSites()
            .firstOrNull { it.url == url } ?: return

        store.dispatch(
            MenuAction.UpdatePinnedState(
                isPinned = true,
            ),
        )
    }

    private fun setupExtensionState(
        store: Store<MenuState, MenuAction>,
    ) = scope.launch {
        try {
            val addons = addonManager.getAddons()
            val recommendedAddons = addons
                .filter { !it.isInstalled() }
                .shuffled()
                .take(NUMBER_OF_RECOMMENDED_ADDONS_TO_SHOW)

            if (recommendedAddons.isNotEmpty()) {
                store.dispatch(
                    MenuAction.UpdateExtensionState(
                        recommendedAddons = recommendedAddons,
                    ),
                )
            }
        } catch (e: AddonManagerException) {
            logger.error("Failed to query extensions", e)
        }
    }

    private fun addBookmark(
        store: Store<MenuState, MenuAction>,
    ) = scope.launch {
        val browserMenuState = store.state.browserMenuState ?: return@launch

        if (browserMenuState.bookmarkState.isBookmarked) {
            return@launch
        }

        val selectedTab = browserMenuState.selectedTab
        val url = selectedTab.getUrl() ?: return@launch

        val guidToEdit = addBookmarkUseCase(
            url = url,
            title = selectedTab.content.title,
        )

        appStore.dispatch(
            BookmarkAction.BookmarkAdded(
                guidToEdit = guidToEdit,
            ),
        )

        onDismiss()
    }

    private fun addShortcut(
        store: Store<MenuState, MenuAction>,
    ) = scope.launch {
        val browserMenuState = store.state.browserMenuState ?: return@launch

        if (browserMenuState.isPinned) {
            return@launch
        }

        val numPinnedSites = pinnedSiteStorage.getPinnedSites()
            .filter { it is TopSite.Default || it is TopSite.Pinned }.size

        if (numPinnedSites >= topSitesMaxLimit) {
            return@launch
        }

        val selectedTab = browserMenuState.selectedTab
        val url = selectedTab.getUrl() ?: return@launch

        addPinnedSiteUseCase(
            title = selectedTab.content.title,
            url = url,
        )
    }

    private fun removeShortcut(
        store: Store<MenuState, MenuAction>,
    ) = scope.launch {
        val browserMenuState = store.state.browserMenuState ?: return@launch

        if (!browserMenuState.isPinned) {
            return@launch
        }

        val selectedTab = browserMenuState.selectedTab
        val url = selectedTab.getUrl() ?: return@launch
        val topSite = pinnedSiteStorage.getPinnedSites()
            .firstOrNull { it.url == url } ?: return@launch

        removePinnedSitesUseCase(topSite = topSite)
    }

    private fun deleteBrowsingDataAndQuit() = scope.launch {
        appStore.dispatch(AppAction.DeleteAndQuitStarted)
        onDeleteAndQuit()
        onDismiss()
    }

    private fun installAddon(
        addon: Addon,
    ) = scope.launch {
        addonManager.installAddon(
            url = addon.downloadUrl,
            installationMethod = InstallationMethod.MANAGER,
            onError = { e ->
                logger.error("Failed to install addon", e)
            },
        )
    }

    companion object {
        private const val NUMBER_OF_RECOMMENDED_ADDONS_TO_SHOW = 4
    }
}
