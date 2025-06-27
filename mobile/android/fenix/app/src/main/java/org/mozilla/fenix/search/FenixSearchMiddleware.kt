/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

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
import mozilla.components.browser.state.action.AwesomeBarAction
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.compose.browser.toolbar.store.BrowserEditToolbarAction
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarStore
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.feature.search.SearchUseCases.SearchUseCase
import mozilla.components.feature.session.SessionUseCases.LoadUrlUseCase
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.feature.tabs.TabsUseCases.SelectTabUseCase
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import mozilla.components.lib.state.ext.flow
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.BookmarksManagement
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.GleanMetrics.History
import org.mozilla.fenix.GleanMetrics.UnifiedSearch
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.NimbusComponents
import org.mozilla.fenix.components.appstate.AppAction.SearchEngineSelected
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
import org.mozilla.fenix.search.SearchFragmentAction.SearchSuggestionsVisibilityUpdated
import org.mozilla.fenix.search.SearchFragmentAction.SuggestionClicked
import org.mozilla.fenix.search.SearchFragmentAction.SuggestionSelected
import org.mozilla.fenix.search.SearchFragmentAction.UpdateQuery
import org.mozilla.fenix.search.awesomebar.SearchSuggestionsProvidersBuilder
import org.mozilla.fenix.search.awesomebar.toSearchProviderState
import org.mozilla.fenix.utils.Settings
import mozilla.components.lib.state.Action as MVIAction

/**
 * [SearchFragmentStore] [Middleware] that will handle the setup of the search UX and related user interactions.
 *
 * @param engine [Engine] used for speculative connections to search suggestions URLs.
 * @param tabsUseCases [TabsUseCases] used for operations related to current open tabs.
 * @param nimbusComponents [NimbusComponents] used for accessing Nimbus events to use in telemetry.
 * @param settings [Settings] application settings.
 * @param appStore [AppStore] to sync search related data with.
 * @param browserStore [BrowserStore] to sync search related data with.
 * @param toolbarStore [BrowserToolbarStore] used for querying and updating the toolbar state.
 * @param includeSelectedTab Whether to include the currently selected tab in the search suggestions.
 */
@Suppress("LongParameterList")
class FenixSearchMiddleware(
    private val engine: Engine,
    private val tabsUseCases: TabsUseCases,
    private val nimbusComponents: NimbusComponents,
    private val settings: Settings,
    private val appStore: AppStore,
    private val browserStore: BrowserStore,
    private val toolbarStore: BrowserToolbarStore,
    private val includeSelectedTab: Boolean = false,
) : Middleware<SearchFragmentState, SearchFragmentAction>, ViewModel() {
    private lateinit var dependencies: LifecycleDependencies
    internal lateinit var searchStore: SearchFragmentStore
    private var observeSearchEnginesChangeJob: Job? = null

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
                observeSearchEngineSelection()
            }

            is UpdateQuery -> {
                next(action)

                maybeShowSearchSuggestions(action.query)
            }

            is SearchEnginesSelectedActions -> {
                next(action)

                updateSearchProviders()
                maybeShowFxSuggestions()
            }

            is SearchProvidersUpdated -> {
                next(action)

                maybeShowSearchSuggestions(searchStore.state.query)
            }

            is SuggestionClicked -> {
                val suggestion = action.suggestion
                when {
                    suggestion.flags.contains(AwesomeBar.Suggestion.Flag.HISTORY) -> {
                        History.searchResultTapped.record(NoExtras())
                    }
                    suggestion.flags.contains(AwesomeBar.Suggestion.Flag.BOOKMARK) -> {
                        BookmarksManagement.searchResultTapped.record(NoExtras())
                    }
                }
                suggestion.onSuggestionClicked?.invoke()
                browserStore.dispatch(AwesomeBarAction.SuggestionClicked(suggestion))
                toolbarStore.dispatch(BrowserEditToolbarAction.SearchQueryUpdated(""))
            }

            is SuggestionSelected -> {
                action.suggestion.editSuggestion?.let {
                    toolbarStore.dispatch(BrowserEditToolbarAction.SearchQueryUpdated(it))
                }
            }

            else -> next(action)
        }
    }

    /**
     * Observe when the user changes the search engine to use for the current in-progress search
     * and update the suggestions providers used and shown suggestions accordingly.
     */
    private fun observeSearchEngineSelection() {
        observeSearchEnginesChangeJob?.cancel()
        observeSearchEnginesChangeJob = appStore.observeWhileActive(dependencies.lifecycleOwner) {
            distinctUntilChangedBy { it.shortcutSearchEngine }
                .collect {
                    it.shortcutSearchEngine?.let {
                        handleSearchShortcutEngineSelectedByUser(it)
                    }
                }
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

    /**
     * Check if new firefox suggestions (trending, recent searches or search engines suggestions)
     * should be shown based on the current search query.
     */
    private fun maybeShowFxSuggestions() {
        val shouldShowSuggestions = with(searchStore.state) {
            (showTrendingSearches || showRecentSearches || showShortcutsSuggestions) &&
                (query.isNotEmpty() || FxNimbus.features.searchSuggestionsOnHomepage.value().enabled)
        }
        searchStore.dispatch(SearchSuggestionsVisibilityUpdated(shouldShowSuggestions))
    }

    /**
     * Check if new search suggestions should be shown based on the current search query.
     */
    private fun maybeShowSearchSuggestions(query: String) {
        val shouldShowSuggestions = with(searchStore.state) {
            url != query && query.isNotBlank() || showSearchShortcuts
        }
        searchStore.dispatch(SearchSuggestionsVisibilityUpdated(shouldShowSuggestions))
    }

    /**
     * Update the search providers used and shown suggestions based on the current search state.
     */
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

    /**
     * Handle a search shortcut engine being selected by the user.
     * This will result in using a different set of suggestions providers and showing different search suggestions.
     * The difference between this and [handleSearchShortcutEngineSelected] is that this also
     * records the appropriate telemetry for the user interaction.
     *
     * @param searchEngine The [SearchEngine] to be used for the current in-progress search.
     */
    @VisibleForTesting
    internal fun handleSearchShortcutEngineSelectedByUser(
        searchEngine: SearchEngine,
    ) {
        handleSearchShortcutEngineSelected(searchEngine)

        UnifiedSearch.engineSelected.record(UnifiedSearch.EngineSelectedExtra(searchEngine.telemetryName()))
    }

    /**
     * Update what search engine to use for the current in-progress search.
     * This will result in using a different set of suggestions providers and showing different search suggestions.
     *
     * @param searchEngine The [SearchEngine] to be used for the current in-progress search.
     */
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
    }

    private fun handleSearchEngineSuggestionClicked(searchEngine: SearchEngine) {
        appStore.dispatch(SearchEngineSelected(searchEngine))
    }

    @VisibleForTesting
    internal fun handleClickSearchEngineSettings() {
        val directions = SearchDialogFragmentDirections.actionGlobalSearchEngineFragment()
        dependencies.navController.navigateSafe(R.id.searchDialogFragment, directions)
        browserStore.dispatch(AwesomeBarAction.EngagementFinished(abandoned = true))
    }

    private inline fun <S : State, A : MVIAction> Store<S, A>.observeWhileActive(
        lifecycleOwner: LifecycleOwner,
        crossinline observe: suspend (Flow<S>.() -> Unit),
    ): Job = with(lifecycleOwner) {
        lifecycleScope.launch {
            repeatOnLifecycle(RESUMED) {
                flow().observe()
            }
        }
    }

    override fun onCleared() {
        observeSearchEnginesChangeJob?.cancel()
        super.onCleared()
    }

    /**
     * Lifecycle dependencies for the [FenixSearchMiddleware].
     *
     * @property context Activity [Context] used for various system interactions.
     * @property lifecycleOwner [LifecycleOwner] depending on which lifecycle related operations will be scheduled.
     * @property browsingModeManager [BrowsingModeManager] for querying the current browsing mode.
     * @property navController [NavController] used to navigate to other destinations.
     * @property fenixBrowserUseCases [FenixBrowserUseCases] used for loading new URLs.
     */
    data class LifecycleDependencies(
        val context: Context,
        val lifecycleOwner: LifecycleOwner,
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
         * @param appStore [AppStore] used for querying application's state related to search.
         * @param browserStore [BrowserStore] used for updating search related data.
         * @param toolbarStore [BrowserToolbarStore] used for querying and updating the toolbar state.
         * @param includeSelectedTab Whether to include the currently selected tab in the search suggestions.
         */
        @Suppress("LongParameterList")
        fun viewModelFactory(
            engine: Engine,
            tabsUseCases: TabsUseCases,
            nimbusComponents: NimbusComponents,
            settings: Settings,
            appStore: AppStore,
            browserStore: BrowserStore,
            toolbarStore: BrowserToolbarStore,
            includeSelectedTab: Boolean,
        ): ViewModelProvider.Factory = object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T = FenixSearchMiddleware(
                engine = engine,
                tabsUseCases = tabsUseCases,
                nimbusComponents = nimbusComponents,
                settings = settings,
                appStore = appStore,
                browserStore = browserStore,
                toolbarStore = toolbarStore,
                includeSelectedTab = includeSelectedTab,
            ) as? T ?: throw IllegalArgumentException("Unknown ViewModel class")
        }
    }
}
