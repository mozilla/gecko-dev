/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.navigation.NavController
import mozilla.components.browser.state.action.AwesomeBarAction
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.feature.search.SearchUseCases.SearchUseCase
import mozilla.components.feature.session.SessionUseCases.LoadUrlUseCase
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.feature.tabs.TabsUseCases.SelectTabUseCase
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.GleanMetrics.UnifiedSearch
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.components.NimbusComponents
import org.mozilla.fenix.components.metrics.MetricsUtils
import org.mozilla.fenix.components.search.BOOKMARKS_SEARCH_ENGINE_ID
import org.mozilla.fenix.components.search.HISTORY_SEARCH_ENGINE_ID
import org.mozilla.fenix.components.search.TABS_SEARCH_ENGINE_ID
import org.mozilla.fenix.components.usecases.FenixBrowserUseCases
import org.mozilla.fenix.ext.navigateSafe
import org.mozilla.fenix.ext.telemetryName
import org.mozilla.fenix.nimbus.FxNimbus
import org.mozilla.fenix.search.SearchFragmentAction.Init
import org.mozilla.fenix.search.SearchFragmentAction.SearchEnginesSelectedActions
import org.mozilla.fenix.search.SearchFragmentAction.SearchProvidersUpdated
import org.mozilla.fenix.search.SearchFragmentAction.SearchStarted
import org.mozilla.fenix.search.awesomebar.SearchSuggestionsProvidersBuilder
import org.mozilla.fenix.search.awesomebar.toSearchProviderState
import org.mozilla.fenix.utils.Settings

/**
 * [SearchFragmentStore] [Middleware] that will handle the setup of the search UX and related user interactions.
 *
 * @param engine [Engine] used for speculative connections to search suggestions URLs.
 * @param tabsUseCases [TabsUseCases] used for operations related to current open tabs.
 * @param nimbusComponents [NimbusComponents] used for accessing Nimbus events to use in telemetry.
 * @param settings [Settings] application settings.
 * @param browserStore [BrowserStore] used for updating search related data.
 * @param includeSelectedTab Whether to include the currently selected tab in the search suggestions.
 */
class FenixSearchMiddleware(
    private val engine: Engine,
    private val tabsUseCases: TabsUseCases,
    private val nimbusComponents: NimbusComponents,
    private val settings: Settings,
    private val browserStore: BrowserStore,
    private val includeSelectedTab: Boolean = false,
) : Middleware<SearchFragmentState, SearchFragmentAction>, ViewModel() {
    private lateinit var dependencies: LifecycleDependencies
    internal lateinit var searchStore: SearchFragmentStore

    @VisibleForTesting
    internal lateinit var suggestionsProvidersBuilder: SearchSuggestionsProvidersBuilder

    /**
     * Updates the [LifecycleDependencies] for this middleware.
     *
     * @param dependencies The new [LifecycleDependencies].
     */
    fun updateLifecycleDependencies(dependencies: LifecycleDependencies) {
        this.dependencies = dependencies
        if (::searchStore.isInitialized) {
            suggestionsProvidersBuilder = buildSearchSuggestionsProvider()
            updateSearchProviders()
        }
    }

    override fun invoke(
        context: MiddlewareContext<SearchFragmentState, SearchFragmentAction>,
        next: (SearchFragmentAction) -> Unit,
        action: SearchFragmentAction,
    ) {
        when (action) {
            is Init -> {
                searchStore = context.store as SearchFragmentStore

                context.dispatch(
                    SearchFragmentAction.UpdateSearchState(
                        browserStore.state.search,
                        true,
                    ),
                )

                next(action)
            }

            is SearchStarted -> {
                next(action)

                engine.speculativeCreateSession(action.inPrivateMode)
                suggestionsProvidersBuilder = buildSearchSuggestionsProvider()
                setSearchEngine(action.selectedSearchEngine)
            }

            is SearchFragmentAction.UpdateQuery -> {
                next(action)

                val shouldShowSuggestions = with(searchStore.state) {
                    url != action.query && action.query.isNotBlank() || showSearchShortcuts
                }
                searchStore.dispatch(SearchFragmentAction.SearchSuggestionsVisibilityUpdated(shouldShowSuggestions))
            }

            is SearchEnginesSelectedActions -> {
                next(action)

                updateSearchProviders()
                maybeShowSearchSuggestions()
            }

            else -> next(action)
        }
    }

    /**
     * Update the search engine to the one selected by the user or fallback to the default search engine.
     *
     * @param searchEngine The new [SearchEngine] to be used for new searches or `null` to fallback to
     * fallback to the default search engine.
     */
    private fun setSearchEngine(searchEngine: SearchEngine?) {
        searchEngine
            ?.let { handleSearchShortcutEngineSelectedByUser(it) }
            ?: searchStore.state.defaultEngine
                ?.let { handleSearchShortcutEngineSelected(it) }
    }

    private fun maybeShowSearchSuggestions() {
        val shouldShowSuggestions = with(searchStore.state) {
            (showTrendingSearches || showRecentSearches || showShortcutsSuggestions) &&
                (query.isNotEmpty() || FxNimbus.features.searchSuggestionsOnHomepage.value().enabled)
        }
        searchStore.dispatch(SearchFragmentAction.SearchSuggestionsVisibilityUpdated(shouldShowSuggestions))
    }

    /**
     * Handle a search shortcut engine being selected by the user.
     *
     * @param searchEngine The [SearchEngine] to be used for new searches.
     */
    private fun handleSearchShortcutEngineSelectedByUser(
        searchEngine: SearchEngine,
    ) {
        handleSearchShortcutEngineSelected(searchEngine)

        UnifiedSearch.engineSelected.record(UnifiedSearch.EngineSelectedExtra(searchEngine.telemetryName()))
    }

    private fun updateSearchProviders() {
        searchStore.dispatch(
            SearchProvidersUpdated(
                buildList {
                    if (searchStore.state.showSearchShortcuts) {
                        add(suggestionsProvidersBuilder.shortcutsEnginePickerProvider)
                    }
                    addAll((suggestionsProvidersBuilder.getProvidersToAdd(searchStore.state.toSearchProviderState())))
                },
            ),
        )
    }

    @VisibleForTesting
    internal fun buildSearchSuggestionsProvider() = SearchSuggestionsProvidersBuilder(
        context = dependencies.context,
        browsingModeManager = dependencies.browsingModeManager,
        includeSelectedTab = includeSelectedTab,
        loadUrlUseCase = loadUrlUseCase,
        searchUseCase = searchUseCase,
        selectTabUseCase = selectTabUseCase,
        onSearchEngineShortcutSelected = ::handleSearchEngineSuggestionClicked,
        onSearchEngineSuggestionSelected = ::handleSearchEngineSuggestionClicked,
        onSearchEngineSettingsClicked = ::handleClickSearchEngineSettings,
    )

    @VisibleForTesting
    internal val loadUrlUseCase = object : LoadUrlUseCase {
        override fun invoke(
            url: String,
            flags: LoadUrlFlags,
            additionalHeaders: Map<String, String>?,
            originalInput: String?,
        ) {
            openToBrowserAndLoad(
                url = url,
                createNewTab = if (settings.enableHomepageAsNewTab) {
                    false
                } else {
                    searchStore.state.tabId == null
                },
                usePrivateMode = dependencies.browsingModeManager.mode.isPrivate,
                flags = flags,
            )

            Events.enteredUrl.record(Events.EnteredUrlExtra(autocomplete = false))

            browserStore.dispatch(AwesomeBarAction.EngagementFinished(abandoned = false))
        }
    }

    @VisibleForTesting
    internal val searchUseCase = object : SearchUseCase {
        override fun invoke(
            searchTerms: String,
            searchEngine: SearchEngine?,
            parentSessionId: String?,
        ) {
            val searchEngine = searchStore.state.searchEngineSource.searchEngine

            openToBrowserAndLoad(
                url = searchTerms,
                createNewTab = if (settings.enableHomepageAsNewTab) {
                    false
                } else {
                    searchStore.state.tabId == null
                },
                usePrivateMode = dependencies.browsingModeManager.mode.isPrivate,
                forceSearch = true,
                searchEngine = searchEngine,
            )

            val searchAccessPoint = when (searchStore.state.searchAccessPoint) {
                MetricsUtils.Source.NONE -> MetricsUtils.Source.SUGGESTION
                else -> searchStore.state.searchAccessPoint
            }

            if (searchEngine != null) {
                MetricsUtils.recordSearchMetrics(
                    searchEngine,
                    searchEngine == searchStore.state.defaultEngine,
                    searchAccessPoint,
                    nimbusComponents.events,
                )
            }

            browserStore.dispatch(AwesomeBarAction.EngagementFinished(abandoned = false))
        }
    }

    @VisibleForTesting
    internal val selectTabUseCase = object : SelectTabUseCase {
        override fun invoke(tabId: String) {
            tabsUseCases.selectTab(tabId)

            dependencies.navController.navigate(R.id.browserFragment)

            browserStore.dispatch(AwesomeBarAction.EngagementFinished(abandoned = false))
        }
    }

    private fun openToBrowserAndLoad(
        url: String,
        createNewTab: Boolean,
        usePrivateMode: Boolean,
        forceSearch: Boolean = false,
        searchEngine: SearchEngine? = null,
        flags: LoadUrlFlags = LoadUrlFlags.none(),
    ) {
        dependencies.navController.navigate(R.id.browserFragment)
        dependencies.fenixBrowserUseCases.loadUrlOrSearch(
            searchTermOrURL = url,
            newTab = createNewTab,
            private = usePrivateMode,
            forceSearch = forceSearch,
            searchEngine = searchEngine,
            flags = flags,
        )
    }

    private fun handleSearchShortcutEngineSelected(searchEngine: SearchEngine) {
        when {
            searchEngine.type == SearchEngine.Type.APPLICATION && searchEngine.id == HISTORY_SEARCH_ENGINE_ID -> {
                searchStore.dispatch(SearchFragmentAction.SearchHistoryEngineSelected(searchEngine))
            }
            searchEngine.type == SearchEngine.Type.APPLICATION && searchEngine.id == BOOKMARKS_SEARCH_ENGINE_ID -> {
                searchStore.dispatch(SearchFragmentAction.SearchBookmarksEngineSelected(searchEngine))
            }
            searchEngine.type == SearchEngine.Type.APPLICATION && searchEngine.id == TABS_SEARCH_ENGINE_ID -> {
                searchStore.dispatch(SearchFragmentAction.SearchTabsEngineSelected(searchEngine))
            }
            searchEngine == searchStore.state.defaultEngine -> {
                searchStore.dispatch(
                    SearchFragmentAction.SearchDefaultEngineSelected(
                        engine = searchEngine,
                        browsingMode = dependencies.browsingModeManager.mode,
                        settings = settings,
                    ),
                )
            }
            else -> {
                searchStore.dispatch(
                    SearchFragmentAction.SearchShortcutEngineSelected(
                        engine = searchEngine,
                        browsingMode = dependencies.browsingModeManager.mode,
                        settings = settings,
                    ),
                )
            }
        }

        updateSearchProviders()
    }

    @VisibleForTesting
    internal fun handleSearchEngineSuggestionClicked(searchEngine: SearchEngine) {
        handleSearchShortcutEngineSelectedByUser(searchEngine)
    }

    @VisibleForTesting
    internal fun handleClickSearchEngineSettings() {
        val directions = SearchDialogFragmentDirections.actionGlobalSearchEngineFragment()
        dependencies.navController.navigateSafe(R.id.searchDialogFragment, directions)
        browserStore.dispatch(AwesomeBarAction.EngagementFinished(abandoned = true))
    }

    /**
     * Lifecycle dependencies for the [FenixSearchMiddleware].
     *
     * @property context Activity [Context] used for various system interactions.
     * @property browsingModeManager [BrowsingModeManager] for querying the current browsing mode.
     * @property navController [NavController] used to navigate to other destinations.
     * @property fenixBrowserUseCases [FenixBrowserUseCases] used for loading new URLs.
     */
    data class LifecycleDependencies(
        val context: Context,
        val browsingModeManager: BrowsingModeManager,
        val navController: NavController,
        val fenixBrowserUseCases: FenixBrowserUseCases,
    )

    /**
     * Static functionalities of the [FenixSearchMiddleware].
     */
    companion object {
        /**
         * [ViewModelProvider.Factory] for creating a [FenixSearchMiddleware].
         *
         * @param engine [Engine] used for speculative connections to search suggestions URLs.
         * @param tabsUseCases [TabsUseCases] used for operations related to current open tabs.
         * @param nimbusComponents [NimbusComponents] used for accessing Nimbus events to use in telemetry.
         * @param settings [Settings] application settings.
         * @param browserStore [BrowserStore] used for updating search related data.
         * @param includeSelectedTab Whether to include the currently selected tab in the search suggestions.
         */
        fun viewModelFactory(
            engine: Engine,
            tabsUseCases: TabsUseCases,
            nimbusComponents: NimbusComponents,
            settings: Settings,
            browserStore: BrowserStore,
            includeSelectedTab: Boolean,
        ): ViewModelProvider.Factory = object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T = FenixSearchMiddleware(
                engine = engine,
                tabsUseCases = tabsUseCases,
                nimbusComponents = nimbusComponents,
                settings = settings,
                browserStore = browserStore,
                includeSelectedTab = includeSelectedTab,
            ) as? T ?: throw IllegalArgumentException("Unknown ViewModel class")
        }
    }
}
