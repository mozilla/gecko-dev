/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import android.content.res.Resources
import androidx.annotation.VisibleForTesting
import androidx.core.graphics.drawable.toDrawable
import androidx.lifecycle.Lifecycle.State.RESUMED
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
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
import mozilla.components.compose.browser.toolbar.store.BrowserEditToolbarAction.SearchQueryUpdated
import mozilla.components.compose.browser.toolbar.store.BrowserEditToolbarAction.UrlSuggestionAutocompleted
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarAction
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarAction.ToggleEditMode
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarEvent
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarMenu
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarState
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarStore
import mozilla.components.compose.browser.toolbar.store.EnvironmentCleared
import mozilla.components.compose.browser.toolbar.store.EnvironmentRehydrated
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
import org.mozilla.fenix.home.toolbar.HomeToolbarEnvironment
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
) : Middleware<BrowserToolbarState, BrowserToolbarAction> {
    @VisibleForTesting
    internal var environment: HomeToolbarEnvironment? = null
    private var syncCurrentSearchEngineJob: Job? = null
    private var syncAvailableSearchEnginesJob: Job? = null

    override fun invoke(
        context: MiddlewareContext<BrowserToolbarState, BrowserToolbarAction>,
        next: (BrowserToolbarAction) -> Unit,
        action: BrowserToolbarAction,
    ) {
        next(action)

        when (action) {
            is EnvironmentRehydrated -> {
                environment = action.environment as? HomeToolbarEnvironment

                if (context.store.state.isEditMode()) {
                    syncCurrentSearchEngine(context.store)
                }
            }

            is EnvironmentCleared -> {
                environment = null
                context.store.dispatch(AutocompleteProvidersUpdated(emptyList()))
            }

            is ToggleEditMode -> {
                if (action.editMode) {
                    refreshConfigurationAfterSearchEngineChange(
                        store = context.store,
                        searchEngine = appStore.state.selectedSearchEngine?.shortcutSearchEngine
                            ?: browserStore.state.search.selectedOrDefaultSearchEngine,
                    )
                    syncCurrentSearchEngine(context.store)
                    syncAvailableEngines(context.store)
                } else {
                    syncCurrentSearchEngineJob?.cancel()
                    syncAvailableSearchEnginesJob?.cancel()
                }
            }

            is SearchSelectorClicked -> {
                UnifiedSearch.searchMenuTapped.record(NoExtras())
            }

            is SearchSettingsItemClicked -> {
                context.store.dispatch(ToggleEditMode(false))
                context.store.dispatch(SearchQueryUpdated(""))
                appStore.dispatch(UpdateSearchBeingActiveState(false))
                browserStore.dispatch(AwesomeBarAction.EngagementFinished(abandoned = true))
                environment?.navController?.navigate(
                    BrowserFragmentDirections.actionGlobalSearchEngineFragment(),
                )
            }

            is SearchSelectorItemClicked -> {
                if (!context.store.state.isEditMode()) {
                    context.store.dispatch(ToggleEditMode(true))
                }
                appStore.dispatch(SearchEngineSelected(action.searchEngine, true))
                refreshConfigurationAfterSearchEngineChange(context.store, action.searchEngine)
            }

            is UrlSuggestionAutocompleted -> {
                components.core.engine.speculativeConnect(action.url)
            }

            else -> {
                // no-op.
            }
        }
    }

    private fun refreshConfigurationAfterSearchEngineChange(
        store: Store<BrowserToolbarState, BrowserToolbarAction>,
        searchEngine: SearchEngine?,
    ) {
        updateSearchSelectorMenu(store, searchEngine, browserStore.state.search.searchEngineShortcuts)
        updateAutocompleteProviders(store, searchEngine)
    }

    private fun updateSearchSelectorMenu(
        store: Store<BrowserToolbarState, BrowserToolbarAction>,
        selectedSearchEngine: SearchEngine?,
        searchEngineShortcuts: List<SearchEngine>,
    ) {
        val environment = environment ?: return

        val searchSelector = buildSearchSelector(
            selectedSearchEngine, searchEngineShortcuts, environment.context.resources,
        )
        store.dispatch(
            SearchActionsStartUpdated(
                when (searchSelector == null) {
                    true -> emptyList()
                    else -> listOf(searchSelector)
                },
            ),
        )
    }

    /**
     * Synchronously update the toolbar with new autocomplete providers suitable for the selected search engine.
     */
    private fun updateAutocompleteProviders(
        store: Store<BrowserToolbarState, BrowserToolbarAction>,
        selectedSearchEngine: SearchEngine?,
    ) {
        if (!settings.shouldAutocompleteInAwesomebar) return

        val autocompleteProviders = buildAutocompleteProvidersList(selectedSearchEngine)
        store.dispatch(AutocompleteProvidersUpdated(autocompleteProviders))
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

    private fun syncCurrentSearchEngine(store: Store<BrowserToolbarState, BrowserToolbarAction>) {
        syncCurrentSearchEngineJob?.cancel()
        syncCurrentSearchEngineJob = appStore.observeWhileActive {
            distinctUntilChangedBy { it.selectedSearchEngine?.shortcutSearchEngine }
                .collect {
                    it.selectedSearchEngine?.let {
                        refreshConfigurationAfterSearchEngineChange(store, it.shortcutSearchEngine)
                    }
                }
        }
    }

    private fun syncAvailableEngines(store: Store<BrowserToolbarState, BrowserToolbarAction>) {
        syncAvailableSearchEnginesJob?.cancel()
        syncAvailableSearchEnginesJob = browserStore.observeWhileActive {
            distinctUntilChangedBy { it.search.searchEngineShortcuts }
                .collect {
                    refreshConfigurationAfterSearchEngineChange(
                        store = store,
                        searchEngine = it.search.selectedOrDefaultSearchEngine,
                    )
                }
        }
    }

    private inline fun <S : State, A : MVIAction> Store<S, A>.observeWhileActive(
        crossinline observe: suspend (Flow<S>.() -> Unit),
    ): Job? = environment?.viewLifecycleOwner?.run {
        lifecycleScope.launch {
            repeatOnLifecycle(RESUMED) {
                flow().observe()
            }
        }
    }

    /**
     * Static functionalities of the [BrowserToolbarSearchMiddleware].
     */
    companion object {
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
