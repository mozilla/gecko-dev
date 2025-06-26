/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.toolbar

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.lifecycle.Lifecycle.State.RESUMED
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import androidx.navigation.NavController
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.distinctUntilChangedBy
import kotlinx.coroutines.launch
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.compose.browser.toolbar.concept.Action
import mozilla.components.compose.browser.toolbar.concept.Action.ActionButtonRes
import mozilla.components.compose.browser.toolbar.concept.Action.TabCounterAction
import mozilla.components.compose.browser.toolbar.concept.PageOrigin
import mozilla.components.compose.browser.toolbar.concept.PageOrigin.Companion.ContextualMenuOption.LoadFromClipboard
import mozilla.components.compose.browser.toolbar.concept.PageOrigin.Companion.ContextualMenuOption.PasteFromClipboard
import mozilla.components.compose.browser.toolbar.concept.PageOrigin.Companion.PageOriginContextualMenuInteractions.LoadFromClipboardClicked
import mozilla.components.compose.browser.toolbar.concept.PageOrigin.Companion.PageOriginContextualMenuInteractions.PasteFromClipboardClicked
import mozilla.components.compose.browser.toolbar.store.BrowserDisplayToolbarAction.BrowserActionsEndUpdated
import mozilla.components.compose.browser.toolbar.store.BrowserDisplayToolbarAction.PageActionsStartUpdated
import mozilla.components.compose.browser.toolbar.store.BrowserDisplayToolbarAction.PageOriginUpdated
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarAction
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarAction.Init
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarAction.ToggleEditMode
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarEvent
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarMenu
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton.ContentDescription.StringResContentDescription
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton.Icon.DrawableResIcon
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton.Text.StringResText
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarState
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarStore
import mozilla.components.compose.browser.toolbar.store.Mode
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import mozilla.components.lib.state.ext.flow
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.utils.ClipboardHandler
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserAnimator
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingMode.Normal
import org.mozilla.fenix.browser.browsingmode.BrowsingMode.Private
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.UseCases
import org.mozilla.fenix.components.menu.MenuAccessPoint
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.home.HomeFragmentDirections
import org.mozilla.fenix.home.toolbar.DisplayActions.MenuClicked
import org.mozilla.fenix.home.toolbar.PageOriginInteractions.OriginClicked
import org.mozilla.fenix.home.toolbar.TabCounterInteractions.AddNewPrivateTab
import org.mozilla.fenix.home.toolbar.TabCounterInteractions.AddNewTab
import org.mozilla.fenix.home.toolbar.TabCounterInteractions.TabCounterClicked
import org.mozilla.fenix.search.BrowserToolbarSearchMiddleware
import org.mozilla.fenix.search.ext.searchEngineShortcuts
import org.mozilla.fenix.tabstray.Page
import mozilla.components.lib.state.Action as MVIAction
import mozilla.components.ui.icons.R as iconsR

@VisibleForTesting
internal sealed class DisplayActions : BrowserToolbarEvent {
    data object MenuClicked : DisplayActions()
}

@VisibleForTesting
internal sealed class TabCounterInteractions : BrowserToolbarEvent {
    data object TabCounterClicked : TabCounterInteractions()
    data object AddNewTab : TabCounterInteractions()
    data object AddNewPrivateTab : TabCounterInteractions()
}

internal sealed class PageOriginInteractions : BrowserToolbarEvent {
    data object OriginClicked : PageOriginInteractions()
}

/**
 * [Middleware] responsible for configuring and handling interactions with the composable toolbar.
 *
 * This is also a [ViewModel] allowing to be easily persisted between activity restarts.
 *
 * @param appStore [AppStore] to sync from.
 * @param browserStore [BrowserStore] to sync from.
 * @param clipboard [ClipboardHandler] to use for reading from device's clipboard.
 */
class BrowserToolbarMiddleware(
    private val appStore: AppStore,
    private val browserStore: BrowserStore,
    private val clipboard: ClipboardHandler,
) : Middleware<BrowserToolbarState, BrowserToolbarAction>, ViewModel() {
    private lateinit var dependencies: LifecycleDependencies
    private var store: BrowserToolbarStore? = null
    private var syncCurrentSearchEngineJob: Job? = null
    private var observeBrowserSearchStateJob: Job? = null

    /**
     * Updates the [LifecycleDependencies] of this middleware.
     *
     * @param dependencies The new [LifecycleDependencies].
     */
    fun updateLifecycleDependencies(dependencies: LifecycleDependencies) {
        this.dependencies = dependencies

        if (store?.state?.mode == Mode.DISPLAY) {
            observeSearchStateUpdates()
        }
        updateToolbarActionsBasedOnOrientation()
        updateTabsCount()
    }

    override fun invoke(
        context: MiddlewareContext<BrowserToolbarState, BrowserToolbarAction>,
        next: (BrowserToolbarAction) -> Unit,
        action: BrowserToolbarAction,
    ) {
        when (action) {
            is Init -> {
                store = context.store as BrowserToolbarStore
                if (action.mode == Mode.DISPLAY) {
                    observeSearchStateUpdates()
                }
                updateEndBrowserActions()
                updatePageOrigin()

                next(action)
            }

            is ToggleEditMode -> {
                when (action.editMode) {
                    true -> stopSearchStateUpdates()
                    false -> observeSearchStateUpdates()
                }

                next(action)
            }

            is MenuClicked -> {
                dependencies.navController.nav(
                    R.id.homeFragment,
                    HomeFragmentDirections.actionGlobalMenuDialogFragment(
                        accesspoint = MenuAccessPoint.Home,
                    ),
                )
            }

            is TabCounterClicked -> {
                dependencies.navController.nav(
                    R.id.homeFragment,
                    NavGraphDirections.actionGlobalTabsTrayFragment(
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

            is OriginClicked -> {
                store?.dispatch(ToggleEditMode(true))
            }
            is PasteFromClipboardClicked -> {
                openNewTab(searchTerms = clipboard.text)
            }
            is LoadFromClipboardClicked -> {
                clipboard.extractURL()?.let {
                    dependencies.useCases.fenixBrowserUseCases.loadUrlOrSearch(
                        searchTermOrURL = it,
                        newTab = true,
                        private = dependencies.browsingModeManager.mode == Private,
                    )
                    dependencies.navController.navigate(R.id.browserFragment)
                } ?: run {
                    Logger("HomeOriginContextMenu").error("Clipboard contains URL but unable to read text")
                }
            }

            else -> next(action)
        }
    }

    private fun openNewTab(
        browsingMode: BrowsingMode? = null,
        searchTerms: String? = null,
    ) {
        browsingMode?.let { dependencies.browsingModeManager.mode = it }
        dependencies.navController.nav(
            R.id.homeFragment,
            NavGraphDirections.actionGlobalSearchDialog(
                sessionId = null,
                pastedText = searchTerms,
            ),
            BrowserAnimator.getToolbarNavOptions(dependencies.context),
        )
    }

    private fun getCurrentNumberOfOpenedTabs() = when (dependencies.browsingModeManager.mode) {
        Normal -> browserStore.state.normalTabs.size
        Private -> browserStore.state.privateTabs.size
    }

    private fun observeSearchStateUpdates() {
        syncCurrentSearchEngineJob?.cancel()
        syncCurrentSearchEngineJob = appStore.observeWhileActive {
            distinctUntilChangedBy { it.shortcutSearchEngine }
                .collect {
                    it.shortcutSearchEngine?.let {
                        updateStartPageActions(it)
                    }
                }
        }

        observeBrowserSearchStateJob?.cancel()
        observeBrowserSearchStateJob = browserStore.observeWhileActive {
            distinctUntilChangedBy { it.search.searchEngineShortcuts }
                .collect {
                    updateStartPageActions(it.search.selectedOrDefaultSearchEngine)
                }
        }
    }

    private fun stopSearchStateUpdates() {
        syncCurrentSearchEngineJob?.cancel()
        observeBrowserSearchStateJob?.cancel()
    }

    private fun updateStartPageActions(selectedSearchEngine: SearchEngine?) {
        store?.dispatch(
            PageActionsStartUpdated(
                buildStartPageActions(selectedSearchEngine),
            ),
        )
    }

    private fun updatePageOrigin() {
        store?.dispatch(
            PageOriginUpdated(
                PageOrigin(
                    hint = R.string.search_hint,
                    title = null,
                    url = null,
                    contextualMenuOptions = listOf(PasteFromClipboard, LoadFromClipboard),
                    onClick = OriginClicked,
                ),
            ),
        )
    }

    private fun updateEndBrowserActions() = store?.dispatch(
        BrowserActionsEndUpdated(
            buildEndBrowserActions(getCurrentNumberOfOpenedTabs()),
        ),
    )

    private fun buildStartPageActions(selectedSearchEngine: SearchEngine?) = listOfNotNull(
        BrowserToolbarSearchMiddleware.buildSearchSelector(
            selectedSearchEngine = selectedSearchEngine,
            searchEngineShortcuts = browserStore.state.search.searchEngineShortcuts,
            resources = dependencies.context.resources,
        ),
    )

    private fun buildEndBrowserActions(tabsCount: Int): List<Action> =
        listOf(
            TabCounterAction(
                count = tabsCount,
                contentDescription = dependencies.context.getString(
                    R.string.mozac_tab_counter_open_tab_tray,
                    tabsCount.toString(),
                ),
                showPrivacyMask = dependencies.browsingModeManager.mode == Private,
                onClick = TabCounterClicked,
                onLongClick = buildTabCounterMenu(),
            ),
            ActionButtonRes(
                drawableResId = R.drawable.mozac_ic_ellipsis_vertical_24,
                contentDescription = R.string.content_description_menu,
                onClick = MenuClicked,
            ),
        )

    private fun buildTabCounterMenu() = BrowserToolbarMenu {
        when (dependencies.browsingModeManager.mode) {
            Normal -> listOf(
                BrowserToolbarMenuButton(
                    icon = DrawableResIcon(iconsR.drawable.mozac_ic_private_mode_24),
                    text = StringResText(R.string.mozac_browser_menu_new_private_tab),
                    contentDescription = StringResContentDescription(R.string.mozac_browser_menu_new_private_tab),
                    onClick = AddNewPrivateTab,
                ),
            )

            Private -> listOf(
                BrowserToolbarMenuButton(
                    icon = DrawableResIcon(iconsR.drawable.mozac_ic_plus_24),
                    text = StringResText(R.string.mozac_browser_menu_new_tab),
                    contentDescription = StringResContentDescription(R.string.mozac_browser_menu_new_tab),
                    onClick = AddNewTab,
                ),
            )
        }
    }

    private fun updateToolbarActionsBasedOnOrientation() {
        with(dependencies.lifecycleOwner) {
            lifecycleScope.launch {
                repeatOnLifecycle(RESUMED) {
                    appStore.flow()
                        .distinctUntilChangedBy { it.orientation }
                        .collect {
                            updateEndBrowserActions()
                        }
                }
            }
        }
    }

    private fun updateTabsCount() {
        with(dependencies.lifecycleOwner) {
            this.lifecycleScope.launch {
                repeatOnLifecycle(RESUMED) {
                    browserStore.flow()
                        .distinctUntilChangedBy { it.tabs.size }
                        .collect {
                            updateEndBrowserActions()
                        }
                }
            }
        }
    }

    private inline fun <S : State, A : MVIAction> Store<S, A>.observeWhileActive(
        crossinline observe: suspend (Flow<S>.() -> Unit),
    ): Job = with(dependencies.lifecycleOwner) {
        lifecycleScope.launch {
            repeatOnLifecycle(RESUMED) {
                flow().observe()
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
     * @property useCases [UseCases] helping this integrate with other features of the applications.
     */
    data class LifecycleDependencies(
        val context: Context,
        val lifecycleOwner: LifecycleOwner,
        val navController: NavController,
        val browsingModeManager: BrowsingModeManager,
        val useCases: UseCases,
    )

    /**
     * Static functionalities of the [BrowserToolbarMiddleware].
     */
    companion object {
        /**
         * [ViewModelProvider.Factory] for creating a [BrowserToolbarMiddleware].
         *
         * @param appStore [AppStore] to sync from.
         * @param browserStore [BrowserStore] to sync from.
         * @param clipboard [ClipboardHandler] to use for reading from device's clipboard.
         */
        fun viewModelFactory(
            appStore: AppStore,
            browserStore: BrowserStore,
            clipboard: ClipboardHandler,
        ): ViewModelProvider.Factory = object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T {
                if (modelClass.isAssignableFrom(BrowserToolbarMiddleware::class.java)) {
                    return BrowserToolbarMiddleware(appStore, browserStore, clipboard) as T
                }
                throw IllegalArgumentException("Unknown ViewModel class")
            }
        }
    }
}
