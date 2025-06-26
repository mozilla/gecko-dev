/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import android.content.res.Resources
import androidx.annotation.VisibleForTesting
import androidx.core.graphics.drawable.toDrawable
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
import mozilla.components.browser.state.action.AwesomeBarAction
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.search.SearchEngine.Type.APPLICATION
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.compose.browser.toolbar.BrowserToolbar
import mozilla.components.compose.browser.toolbar.concept.Action.SearchSelectorAction
import mozilla.components.compose.browser.toolbar.store.BrowserEditToolbarAction.AutocompleteProvidersUpdated
import mozilla.components.compose.browser.toolbar.store.BrowserEditToolbarAction.SearchActionsStartUpdated
import mozilla.components.compose.browser.toolbar.store.BrowserEditToolbarAction.UpdateEditText
import mozilla.components.compose.browser.toolbar.store.BrowserEditToolbarAction.UrlSuggestionAutocompleted
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarAction
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarAction.Init
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarAction.ToggleEditMode
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarEvent
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarMenu
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarState
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarStore
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import mozilla.components.lib.state.ext.flow
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.UnifiedSearch
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.components.appstate.AppAction.SearchEngineSelected
import org.mozilla.fenix.components.appstate.AppAction.UpdateSearchBeingActiveState
import org.mozilla.fenix.components.search.BOOKMARKS_SEARCH_ENGINE_ID
import org.mozilla.fenix.components.search.HISTORY_SEARCH_ENGINE_ID
import org.mozilla.fenix.components.search.TABS_SEARCH_ENGINE_ID
import org.mozilla.fenix.search.SearchSelectorEvents.SearchSelectorClicked
import org.mozilla.fenix.search.SearchSelectorEvents.SearchSelectorItemClicked
import org.mozilla.fenix.search.SearchSelectorEvents.SearchSettingsItemClicked
import org.mozilla.fenix.search.ext.searchEngineShortcuts
import org.mozilla.fenix.utils.Settings
import mozilla.components.compose.browser.toolbar.concept.Action.SearchSelectorAction.ContentDescription.StringContentDescription as SearchSelectorDescription
import mozilla.components.compose.browser.toolbar.concept.Action.SearchSelectorAction.Icon.DrawableIcon as SearchSelectorIcon
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton.ContentDescription.StringContentDescription as MenuItemStringDescription
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton.ContentDescription.StringResContentDescription as MenuItemDescriptionRes
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton.Icon.DrawableIcon as MenuItemIcon
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton.Icon.DrawableResIcon as MenuItemIconRes
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton.Text.StringResText as MenuItemStringResText
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton.Text.StringText as MenuItemStringText
import mozilla.components.lib.state.Action as MVIAction

@VisibleForTesting
internal sealed class SearchSelectorEvents : BrowserToolbarEvent {
    data object SearchSelectorClicked : SearchSelectorEvents()

    data object SearchSettingsItemClicked : SearchSelectorEvents()

    data class SearchSelectorItemClicked(
        val searchEngine: SearchEngine,
    ) : SearchSelectorEvents()
}

/**
 * [BrowserToolbarStore] middleware handling the configuration of the composable toolbar
 * while in edit mode.
 *
 * @param appStore [AppStore] used for querying and updating application state.
 * @param browserStore [BrowserStore] used for querying and updating browser state.
 * @param components [Components] for accessing other functionalities of the application.
 * @param settings [Settings] for accessing application settings.
 */
class BrowserToolbarSearchMiddleware(
    private val appStore: AppStore,
    private val browserStore: BrowserStore,
    private val components: Components,
    private val settings: Settings,
) : Middleware<BrowserToolbarState, BrowserToolbarAction>, ViewModel() {
    private lateinit var toolbarStore: BrowserToolbarStore
    private lateinit var dependencies: LifecycleDependencies
    private var syncCurrentSearchEngineJob: Job? = null

    /**
     * Updates the [LifecycleDependencies] of this middleware.
     *
     * @param dependencies The new [LifecycleDependencies].
     */
    fun updateLifecycleDependencies(dependencies: LifecycleDependencies) {
        this.dependencies = dependencies
        if (syncCurrentSearchEngineJob?.isCancelled == false) {
            syncCurrentSearchEngine()
        }
    }

    override fun invoke(
        context: MiddlewareContext<BrowserToolbarState, BrowserToolbarAction>,
        next: (BrowserToolbarAction) -> Unit,
        action: BrowserToolbarAction,
    ) {
        next(action)

        when (action) {
            is Init -> {
                toolbarStore = context.store as BrowserToolbarStore
            }

            is ToggleEditMode -> {
                if (action.editMode == true) {
                    refreshConfigurationAfterSearchEngineChange(
                        appStore.state.shortcutSearchEngine ?: browserStore.state.search.selectedOrDefaultSearchEngine,
                    )
                    syncCurrentSearchEngine()
                } else {
                    syncCurrentSearchEngineJob?.cancel()
                }
            }

            is SearchSelectorClicked -> {
                UnifiedSearch.searchMenuTapped.record(NoExtras())
            }

            is SearchSettingsItemClicked -> {
                toolbarStore.dispatch(ToggleEditMode(false))
                toolbarStore.dispatch(UpdateEditText(""))
                appStore.dispatch(UpdateSearchBeingActiveState(false))
                browserStore.dispatch(AwesomeBarAction.EngagementFinished(abandoned = true))
                dependencies.navController.navigate(
                    BrowserFragmentDirections.actionGlobalSearchEngineFragment(),
                )
            }

            is SearchSelectorItemClicked -> {
                if (!toolbarStore.state.isEditMode()) {
                    toolbarStore.dispatch(ToggleEditMode(true))
                }
                appStore.dispatch(SearchEngineSelected(action.searchEngine))
                refreshConfigurationAfterSearchEngineChange(action.searchEngine)
            }

            is UrlSuggestionAutocompleted -> {
                components.core.engine.speculativeConnect(action.url)
            }

            else -> {
                // no-op.
            }
        }
    }

    private fun refreshConfigurationAfterSearchEngineChange(searchEngine: SearchEngine?) {
        updateSearchSelectorMenu(searchEngine, browserStore.state.search.searchEngineShortcuts)
        updateAutocompleteProviders(searchEngine)
    }

    private fun updateSearchSelectorMenu(
        selectedSearchEngine: SearchEngine?,
        searchEngineShortcuts: List<SearchEngine>,
    ) {
        val searchSelector = buildSearchSelector(selectedSearchEngine, searchEngineShortcuts, dependencies.resources)
        toolbarStore.dispatch(
            SearchActionsStartUpdated(
                when (searchSelector == null) {
                    true -> emptyList()
                    else -> listOf(searchSelector)
                },
            ),
        )
    }

    private fun updateAutocompleteProviders(selectedSearchEngine: SearchEngine?) {
        if (!settings.shouldAutocompleteInAwesomebar) return

        val autocompleteProviders = buildAutocompleteProvidersList(selectedSearchEngine)
        toolbarStore.dispatch(AutocompleteProvidersUpdated(autocompleteProviders))
    }

    private fun buildAutocompleteProvidersList(selectedSearchEngine: SearchEngine?) = when (selectedSearchEngine?.id) {
        browserStore.state.search.selectedOrDefaultSearchEngine?.id -> listOfNotNull(
            when (settings.shouldShowHistorySuggestions) {
                true -> components.core.historyStorage
                false -> null
            },
            when (settings.shouldShowBookmarkSuggestions) {
                true -> components.core.bookmarksStorage
                false -> null
            },
            components.core.domainsAutocompleteProvider,
        )

        TABS_SEARCH_ENGINE_ID -> listOf(
            components.core.sessionAutocompleteProvider,
            components.backgroundServices.syncedTabsAutocompleteProvider,
        )

        BOOKMARKS_SEARCH_ENGINE_ID -> listOf(
            components.core.bookmarksStorage,
        )

        HISTORY_SEARCH_ENGINE_ID -> listOf(
            components.core.historyStorage,
        )

        else -> emptyList()
    }

    private fun syncCurrentSearchEngine() {
        syncCurrentSearchEngineJob?.cancel()
        syncCurrentSearchEngineJob = observeWhileActive(appStore) {
            distinctUntilChangedBy { it.shortcutSearchEngine }
                .collect {
                    it.shortcutSearchEngine?.let {
                        refreshConfigurationAfterSearchEngineChange(it)
                    }
                }
        }
    }

    private inline fun <S : State, A : MVIAction> observeWhileActive(
        store: Store<S, A>,
        crossinline observe: suspend (Flow<S>.() -> Unit),
    ): Job = with(dependencies.lifecycleOwner) {
        lifecycleScope.launch {
            repeatOnLifecycle(RESUMED) {
                store.flow().observe()
            }
        }
    }

    /**
     * Lifecycle dependencies for the [BrowserToolbarSearchMiddleware].
     *
     * @property lifecycleOwner [LifecycleOwner] depending on which lifecycle related operations will be scheduled.
     * @property navController [NavController] used to navigate to other in-app destinations.
     * @property resources [Resources] used for accessing application resources.
     */
    data class LifecycleDependencies(
        val lifecycleOwner: LifecycleOwner,
        val navController: NavController,
        val resources: Resources,
    )

    /**
     * Static functionalities of the [BrowserToolbarSearchMiddleware].
     */
    companion object {
        /**
         * [ViewModelProvider.Factory] for creating a [BrowserToolbarSearchMiddleware].
         *
         * @param appStore The [AppStore] to sync search related data with.
         * @param browserStore The [BrowserStore] to sync search related data with.
         * @param components [Components] for accessing other functionalities of the application.
         * @param settings [Settings] for accessing application settings.
         */
        fun viewModelFactory(
            appStore: AppStore,
            browserStore: BrowserStore,
            components: Components,
            settings: Settings,
        ): ViewModelProvider.Factory = object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T =
                BrowserToolbarSearchMiddleware(
                    appStore,
                    browserStore,
                    components,
                    settings,
                ) as? T ?: throw IllegalArgumentException("Unknown ViewModel class")
        }

        /**
         * Builds a [SearchSelectorAction] to be shown in the [BrowserToolbar].
         *
         * @param selectedSearchEngine The currently selected search engine.
         * @param searchEngineShortcuts The list of search engines available for selection.
         * @param resources [Resources] Used for accessing application resources.
         */
        fun buildSearchSelector(
            selectedSearchEngine: SearchEngine?,
            searchEngineShortcuts: List<SearchEngine>,
            resources: Resources,
        ): SearchSelectorAction? {
            if (selectedSearchEngine == null) {
                return null
            }

            val menuItems = buildList<BrowserToolbarMenuItem> {
                add(
                    BrowserToolbarMenuButton(
                        icon = null,
                        text = MenuItemStringResText(R.string.search_header_menu_item_2),
                        contentDescription = MenuItemDescriptionRes(R.string.search_header_menu_item_2),
                        onClick = null,
                    ),
                )
                addAll(searchEngineShortcuts.toToolbarMenuItems(resources))
                add(
                    BrowserToolbarMenuButton(
                        icon = MenuItemIconRes(R.drawable.mozac_ic_settings_24),
                        text = MenuItemStringResText(R.string.search_settings_menu_item),
                        contentDescription = MenuItemDescriptionRes(R.string.search_settings_menu_item),
                        onClick = SearchSettingsItemClicked,
                    ),
                )
            }

            return SearchSelectorAction(
                icon = SearchSelectorIcon(
                    drawable = selectedSearchEngine.icon.toDrawable(resources),
                    shouldTint = selectedSearchEngine.type == APPLICATION,
                ),
                contentDescription = SearchSelectorDescription(
                    resources.getString(
                        R.string.search_engine_selector_content_description,
                        selectedSearchEngine.name,
                    ),
                ),
                menu = BrowserToolbarMenu { menuItems },
                onClick = SearchSelectorClicked,
            )
        }

        private fun List<SearchEngine>.toToolbarMenuItems(
            resources: Resources,
        ) = map { searchEngine ->
            BrowserToolbarMenuButton(
                icon = MenuItemIcon(
                    drawable = searchEngine.icon.toDrawable(resources),
                    shouldTint = searchEngine.type == APPLICATION,
                ),
                text = MenuItemStringText(searchEngine.name),
                contentDescription = MenuItemStringDescription(searchEngine.name),
                onClick = SearchSelectorItemClicked(searchEngine),
            )
        }
    }
}
