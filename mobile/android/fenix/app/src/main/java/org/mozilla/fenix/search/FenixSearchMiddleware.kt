/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import androidx.annotation.VisibleForTesting
import androidx.lifecycle.Lifecycle.State.RESUMED
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
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
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.NimbusComponents
import org.mozilla.fenix.components.UseCases
import org.mozilla.fenix.components.appstate.AppAction.SearchEngineSelected
import org.mozilla.fenix.components.metrics.MetricsUtils
import org.mozilla.fenix.components.search.BOOKMARKS_SEARCH_ENGINE_ID
import org.mozilla.fenix.components.search.HISTORY_SEARCH_ENGINE_ID
import org.mozilla.fenix.components.search.TABS_SEARCH_ENGINE_ID
import org.mozilla.fenix.ext.navigateSafe
import org.mozilla.fenix.ext.telemetryName
import org.mozilla.fenix.nimbus.FxNimbus
import org.mozilla.fenix.search.SearchFragmentAction.EnvironmentCleared
import org.mozilla.fenix.search.SearchFragmentAction.EnvironmentRehydrated
import org.mozilla.fenix.search.SearchFragmentAction.Init
import org.mozilla.fenix.search.SearchFragmentAction.SearchEnginesSelectedActions
import org.mozilla.fenix.search.SearchFragmentAction.SearchProvidersUpdated
import org.mozilla.fenix.search.SearchFragmentAction.SearchStarted
import org.mozilla.fenix.search.SearchFragmentAction.SearchSuggestionsVisibilityUpdated
import org.mozilla.fenix.search.SearchFragmentAction.SuggestionClicked
import org.mozilla.fenix.search.SearchFragmentAction.SuggestionSelected
import org.mozilla.fenix.search.SearchFragmentAction.UpdateQuery
import org.mozilla.fenix.search.SearchFragmentStore.Environment
import org.mozilla.fenix.search.awesomebar.SearchSuggestionsProvidersBuilder
import org.mozilla.fenix.search.awesomebar.toSearchProviderState
import org.mozilla.fenix.utils.Settings
import mozilla.components.lib.state.Action as MVIAction

/**
 * [SearchFragmentStore] [Middleware] that will handle the setup of the search UX and related user interactions.
 *
 * @param engine [Engine] used for speculative connections to search suggestions URLs.
 * @param useCases [UseCases] helping this integrate with other features of the applications.
 * @param nimbusComponents [NimbusComponents] used for accessing Nimbus events to use in telemetry.
 * @param settings [Settings] application settings.
 * @param appStore [AppStore] to sync search related data with.
 * @param browserStore [BrowserStore] to sync search related data with.
 * @param toolbarStore [BrowserToolbarStore] used for querying and updating the toolbar state.
 */
@Suppress("LongParameterList")
class FenixSearchMiddleware(
    private val engine: Engine,
    private val useCases: UseCases,
    private val nimbusComponents: NimbusComponents,
    private val settings: Settings,
    private val appStore: AppStore,
    private val browserStore: BrowserStore,
    private val toolbarStore: BrowserToolbarStore,
) : Middleware<SearchFragmentState, SearchFragmentAction> {
    @VisibleForTesting
    internal var environment: Environment? = null
    private var observeSearchEnginesChangeJob: Job? = null

    @VisibleForTesting
    internal var suggestionsProvidersBuilder: SearchSuggestionsProvidersBuilder? = null

    override fun invoke(
        context: MiddlewareContext<SearchFragmentState, SearchFragmentAction>,
        next: (SearchFragmentAction) -> Unit,
        action: SearchFragmentAction,
    ) {
        when (action) {
            is Init -> {
                context.store.dispatch(
                    SearchFragmentAction.UpdateSearchState(
                        browserStore.state.search,
                        true,
                    ),
                )

                next(action)
            }

            is EnvironmentRehydrated -> {
                next(action)

                environment = action.environment

                suggestionsProvidersBuilder = buildSearchSuggestionsProvider(context.store)
                updateSearchProviders(context.store)
            }

            is EnvironmentCleared -> {
                next(action)

                environment = null

                // Search providers may keep hard references to lifecycle dependent objects
                // so we need to reset them when the environment is cleared.
                suggestionsProvidersBuilder = null
                context.store.dispatch(SearchProvidersUpdated(emptyList()))
            }

            is SearchStarted -> {
                next(action)

                engine.speculativeCreateSession(action.inPrivateMode)
                suggestionsProvidersBuilder = buildSearchSuggestionsProvider(context.store)
                setSearchEngine(context.store, action.selectedSearchEngine, action.isUserSelected)
                observeSearchEngineSelection(context.store)
            }

            is UpdateQuery -> {
                next(action)

                maybeShowSearchSuggestions(context.store, action.query)
            }

            is SearchEnginesSelectedActions -> {
                next(action)

                updateSearchProviders(context.store)
                maybeShowFxSuggestions(context.store)
            }

            is SearchProvidersUpdated -> {
                next(action)

                if (action.providers.isNotEmpty()) {
                    maybeShowSearchSuggestions(context.store, context.store.state.query)
                }
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
                browserStore.dispatch(AwesomeBarAction.SuggestionClicked(suggestion))
                toolbarStore.dispatch(BrowserEditToolbarAction.SearchQueryUpdated(""))
                suggestion.onSuggestionClicked?.invoke()
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
    private fun observeSearchEngineSelection(store: Store<SearchFragmentState, SearchFragmentAction>) {
        observeSearchEnginesChangeJob?.cancel()
        observeSearchEnginesChangeJob = appStore.observeWhileActive {
            distinctUntilChangedBy { it.selectedSearchEngine?.shortcutSearchEngine }
                .collect {
                    it.selectedSearchEngine?.let {
                        when (it.isUserSelected) {
                            true -> handleSearchShortcutEngineSelectedByUser(store, it.shortcutSearchEngine)
                            false -> handleSearchShortcutEngineSelected(store, it.shortcutSearchEngine)
                        }
                    }
                }
        }
    }

    /**
     * Update the search engine to the one selected by the user or fallback to the default search engine.
     *
     * @param store The store which will provide the state and environment dependencies needed.
     * @param searchEngine The new [SearchEngine] to be used for new searches or `null` to fallback to
     * fallback to the default search engine.
     * @param isSelectedByUser isUserSelected Whether or not the search engine was selected by the user.
     */
    private fun setSearchEngine(
        store: Store<SearchFragmentState, SearchFragmentAction>,
        searchEngine: SearchEngine?,
        isSelectedByUser: Boolean,
    ) {
        searchEngine?.let {
            when (isSelectedByUser) {
                true -> handleSearchShortcutEngineSelectedByUser(store, it)
                false -> handleSearchShortcutEngineSelected(store, it)
            }
        } ?: store.state.defaultEngine?.let { handleSearchShortcutEngineSelected(store, it) }
    }

    /**
     * Check if new firefox suggestions (trending, recent searches or search engines suggestions)
     * should be shown based on the current search query.
     */
    private fun maybeShowFxSuggestions(store: Store<SearchFragmentState, SearchFragmentAction>) {
        val shouldShowSuggestions = store.state.run {
            (showTrendingSearches || showRecentSearches || showShortcutsSuggestions) &&
                (query.isNotEmpty() || FxNimbus.features.searchSuggestionsOnHomepage.value().enabled)
        }
        store.dispatch(SearchSuggestionsVisibilityUpdated(shouldShowSuggestions))
    }

    /**
     * Check if new search suggestions should be shown based on the current search query.
     */
    private fun maybeShowSearchSuggestions(
        store: Store<SearchFragmentState, SearchFragmentAction>,
        query: String,
    ) {
        val shouldShowSuggestions = with(store.state) {
            url != query && query.isNotBlank() || showSearchShortcuts
        }

        store.dispatch(SearchSuggestionsVisibilityUpdated(shouldShowSuggestions))
    }

    /**
     * Update the search providers used and shown suggestions based on the current search state.
     */
    private fun updateSearchProviders(store: Store<SearchFragmentState, SearchFragmentAction>) {
        val suggestionsProvidersBuilder = suggestionsProvidersBuilder ?: return
        store.dispatch(
            SearchProvidersUpdated(
                buildList {
                    if (store.state.showSearchShortcuts) {
                        add(suggestionsProvidersBuilder.shortcutsEnginePickerProvider)
                    }
                    addAll((suggestionsProvidersBuilder.getProvidersToAdd(store.state.toSearchProviderState())))
                },
            ),
        )
    }

    @VisibleForTesting
    internal fun buildSearchSuggestionsProvider(
        store: Store<SearchFragmentState, SearchFragmentAction>,
    ): SearchSuggestionsProvidersBuilder? {
        val environment = environment ?: return null

        return SearchSuggestionsProvidersBuilder(
            context = environment.context,
            browsingModeManager = environment.browsingModeManager,
            includeSelectedTab = store.state.tabId == null,
            loadUrlUseCase = loadUrlUseCase(store),
            searchUseCase = searchUseCase(store),
            selectTabUseCase = selectTabUseCase(),
            onSearchEngineShortcutSelected = ::handleSearchEngineSuggestionClicked,
            onSearchEngineSuggestionSelected = ::handleSearchEngineSuggestionClicked,
            onSearchEngineSettingsClicked = { handleClickSearchEngineSettings() },
        )
    }

    @VisibleForTesting
    internal fun loadUrlUseCase(store: Store<SearchFragmentState, SearchFragmentAction>) = object : LoadUrlUseCase {
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
                    store.state.tabId == null
                },
                usePrivateMode = environment?.browsingModeManager?.mode?.isPrivate == true,
                flags = flags,
            )

            Events.enteredUrl.record(Events.EnteredUrlExtra(autocomplete = false))

            browserStore.dispatch(AwesomeBarAction.EngagementFinished(abandoned = false))
        }
    }

    @VisibleForTesting
    internal fun searchUseCase(store: Store<SearchFragmentState, SearchFragmentAction>) = object : SearchUseCase {
        override fun invoke(
            searchTerms: String,
            searchEngine: SearchEngine?,
            parentSessionId: String?,
        ) {
            val searchEngine = store.state.searchEngineSource.searchEngine

            openToBrowserAndLoad(
                url = searchTerms,
                createNewTab = if (settings.enableHomepageAsNewTab) {
                    false
                } else {
                    store.state.tabId == null
                },
                usePrivateMode = environment?.browsingModeManager?.mode?.isPrivate == true,
                forceSearch = true,
                searchEngine = searchEngine,
            )

            val searchAccessPoint = when (store.state.searchAccessPoint) {
                MetricsUtils.Source.NONE -> MetricsUtils.Source.SUGGESTION
                else -> store.state.searchAccessPoint
            }

            if (searchEngine != null) {
                MetricsUtils.recordSearchMetrics(
                    searchEngine,
                    searchEngine == store.state.defaultEngine,
                    searchAccessPoint,
                    nimbusComponents.events,
                )
            }

            browserStore.dispatch(AwesomeBarAction.EngagementFinished(abandoned = false))
        }
    }

    @VisibleForTesting
    internal fun selectTabUseCase() = object : SelectTabUseCase {
        override fun invoke(tabId: String) {
            useCases.tabsUseCases.selectTab(tabId)

            environment?.navController?.navigate(R.id.browserFragment)

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
        environment?.navController?.navigate(R.id.browserFragment)
        useCases.fenixBrowserUseCases.loadUrlOrSearch(
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
     * @param store The store which will provide the state and environment dependencies needed.
     * @param searchEngine The [SearchEngine] to be used for the current in-progress search.
     */
    @VisibleForTesting
    internal fun handleSearchShortcutEngineSelectedByUser(
        store: Store<SearchFragmentState, SearchFragmentAction>,
        searchEngine: SearchEngine,
    ) {
        handleSearchShortcutEngineSelected(store, searchEngine)

        UnifiedSearch.engineSelected.record(UnifiedSearch.EngineSelectedExtra(searchEngine.telemetryName()))
    }

    /**
     * Update what search engine to use for the current in-progress search.
     * This will result in using a different set of suggestions providers and showing different search suggestions.
     *
     * @param store The store which will provide the state and environment dependencies needed.
     * @param searchEngine The [SearchEngine] to be used for the current in-progress search.
     */
    private fun handleSearchShortcutEngineSelected(
        store: Store<SearchFragmentState, SearchFragmentAction>,
        searchEngine: SearchEngine,
    ) {
        val environment = environment ?: return

        when {
            searchEngine.type == SearchEngine.Type.APPLICATION && searchEngine.id == HISTORY_SEARCH_ENGINE_ID -> {
                store.dispatch(SearchFragmentAction.SearchHistoryEngineSelected(searchEngine))
            }
            searchEngine.type == SearchEngine.Type.APPLICATION && searchEngine.id == BOOKMARKS_SEARCH_ENGINE_ID -> {
                store.dispatch(SearchFragmentAction.SearchBookmarksEngineSelected(searchEngine))
            }
            searchEngine.type == SearchEngine.Type.APPLICATION && searchEngine.id == TABS_SEARCH_ENGINE_ID -> {
                store.dispatch(SearchFragmentAction.SearchTabsEngineSelected(searchEngine))
            }
            searchEngine == store.state.defaultEngine -> {
                store.dispatch(
                    SearchFragmentAction.SearchDefaultEngineSelected(
                        engine = searchEngine,
                        browsingMode = environment.browsingModeManager.mode,
                        settings = settings,
                    ),
                )
            }
            else -> {
                store.dispatch(
                    SearchFragmentAction.SearchShortcutEngineSelected(
                        engine = searchEngine,
                        browsingMode = environment.browsingModeManager.mode,
                        settings = settings,
                    ),
                )
            }
        }
    }

    private fun handleSearchEngineSuggestionClicked(searchEngine: SearchEngine) {
        appStore.dispatch(SearchEngineSelected(searchEngine, true))
    }

    @VisibleForTesting
    internal fun handleClickSearchEngineSettings() {
        val directions = SearchDialogFragmentDirections.actionGlobalSearchEngineFragment()
        environment?.navController?.navigateSafe(R.id.searchDialogFragment, directions)
        browserStore.dispatch(AwesomeBarAction.EngagementFinished(abandoned = true))
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
}
