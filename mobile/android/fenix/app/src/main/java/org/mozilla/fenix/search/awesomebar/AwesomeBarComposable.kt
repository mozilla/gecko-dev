/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.awesomebar

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.isImeVisible
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.platform.LocalSoftwareKeyboardController
import androidx.fragment.app.Fragment
import androidx.lifecycle.ViewModelProvider
import androidx.navigation.NavController
import mozilla.components.browser.state.action.AwesomeBarAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.compose.browser.awesomebar.AwesomeBar
import mozilla.components.compose.browser.awesomebar.AwesomeBarDefaults
import mozilla.components.compose.browser.awesomebar.AwesomeBarOrientation
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarAction.ToggleEditMode
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarStore
import mozilla.components.lib.state.ext.observeAsComposableState
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.components.appstate.AppAction.UpdateSearchBeingActiveState
import org.mozilla.fenix.components.metrics.MetricsUtils
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.search.BrowserToolbarToFenixSearchMapperMiddleware
import org.mozilla.fenix.search.FenixSearchMiddleware
import org.mozilla.fenix.search.SearchDialogFragmentStore
import org.mozilla.fenix.search.SearchFragmentAction.SearchSuggestionsVisibilityUpdated
import org.mozilla.fenix.search.SearchFragmentAction.SuggestionClicked
import org.mozilla.fenix.search.SearchFragmentAction.SuggestionSelected
import org.mozilla.fenix.search.SearchFragmentStore
import org.mozilla.fenix.search.createInitialSearchFragmentState
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Wrapper over a [Composable] to show search suggestions, responsible for its setup.
 *
 * @param activity [HomeActivity] providing the ability to open URLs and querying the current browsing mode.
 * @param components [Components] for accessing other functionalities of the application.
 * @param appStore [AppStore] for accessing the current application state.
 * @param browserStore [BrowserStore] for accessing the current browser state.
 * @param toolbarStore [BrowserToolbarStore] for accessing the current toolbar state.
 * @param navController [NavController] for navigating to other destinations in the application.
 * @param lifecycleOwner [Fragment] for controlling the lifetime of long running operations.
 * @param includeSelectedTab Whether to include the currently selected tab in the search suggestions.
 * Defaults to `true`.
 */
@Suppress("LongParameterList")
class AwesomeBarComposable(
    private val activity: HomeActivity,
    private val components: Components,
    private val appStore: AppStore,
    private val browserStore: BrowserStore,
    private val toolbarStore: BrowserToolbarStore,
    private val navController: NavController,
    private val lifecycleOwner: Fragment,
    private val includeSelectedTab: Boolean = true,
) {
    private val toolbarQueryMapper = getOrCreate<BrowserToolbarToFenixSearchMapperMiddleware>()
    private val searchMiddleware = getOrCreate<FenixSearchMiddleware>()
    private val store = getOrCreate<SearchDialogFragmentStore>()

    /**
     * [Composable] fully integrated with [BrowserStore] and [BrowserToolbarStore]
     * that will show search suggestions whenever the users edits the current query in the toolbar.
     */
    @OptIn(ExperimentalLayoutApi::class) // for WindowInsets.isImeVisible
    @Composable
    fun SearchSuggestions() {
        val isSearchActive = appStore.observeAsComposableState { it.isSearchActive }.value
        val state = store.observeAsComposableState { it }.value
        val orientation by remember(state.searchSuggestionsOrientedAtBottom) {
            derivedStateOf {
                when (store.state.searchSuggestionsOrientedAtBottom) {
                    true -> AwesomeBarOrientation.BOTTOM
                    false -> AwesomeBarOrientation.TOP
                }
            }
        }
        val focusManager = LocalFocusManager.current
        val keyboardController = LocalSoftwareKeyboardController.current

        LaunchedEffect(isSearchActive) {
            if (!isSearchActive) {
                appStore.dispatch(UpdateSearchBeingActiveState(false))
                focusManager.clearFocus()
                keyboardController?.hide()
            }
        }

        BackHandler {
            store.dispatch(SearchSuggestionsVisibilityUpdated(false))
            toolbarStore.dispatch(ToggleEditMode(false))
            browserStore.dispatch(AwesomeBarAction.EngagementFinished(abandoned = true))
        }

        if (state.shouldShowSearchSuggestions) {
            Box(
                modifier = Modifier
                    .background(AcornTheme.colors.layer1)
                    .fillMaxSize()
                    .pointerInput(WindowInsets.isImeVisible) {
                        detectTapGestures(
                            // Hide the keyboard for any touches in the empty area of the awesomebar
                            onPress = { keyboardController?.hide() },
                        )
                    },
            ) {
                AwesomeBar(
                    text = state.query,
                    providers = state.searchSuggestionsProviders,
                    orientation = orientation,
                    colors = AwesomeBarDefaults.colors(
                        background = Color.Transparent,
                        title = FirefoxTheme.colors.textPrimary,
                        description = FirefoxTheme.colors.textSecondary,
                        autocompleteIcon = FirefoxTheme.colors.textSecondary,
                        groupTitle = FirefoxTheme.colors.textSecondary,
                    ),
                    onSuggestionClicked = { suggestion ->
                        store.dispatch(SuggestionClicked(suggestion))
                    },
                    onAutoComplete = { suggestion ->
                        store.dispatch(SuggestionSelected(suggestion))
                    },
                    onVisibilityStateUpdated = {},
                    onScroll = { keyboardController?.hide() },
                    profiler = components.core.engine.profiler,
                )
            }
        }
    }

    private inline fun <reified T> getOrCreate(): T = when (T::class.java) {
        BrowserToolbarToFenixSearchMapperMiddleware::class.java ->
            ViewModelProvider(
                lifecycleOwner,
                BrowserToolbarToFenixSearchMapperMiddleware.viewModelFactory(toolbarStore),
            ).get(BrowserToolbarToFenixSearchMapperMiddleware::class.java).also {
                it.updateLifecycleDependencies(
                    BrowserToolbarToFenixSearchMapperMiddleware.LifecycleDependencies(
                        browsingModeManager = activity.browsingModeManager,
                        lifecycleOwner = lifecycleOwner,
                    ),
                )
            } as T

        FenixSearchMiddleware::class.java ->
            ViewModelProvider(
                lifecycleOwner,
                FenixSearchMiddleware.viewModelFactory(
                    engine = components.core.engine,
                    tabsUseCases = components.useCases.tabsUseCases,
                    nimbusComponents = components.nimbus,
                    settings = components.settings,
                    browserStore = browserStore,
                    toolbarStore = toolbarStore,
                    includeSelectedTab = includeSelectedTab,
                ),
            )[FenixSearchMiddleware::class.java].also {
                it.updateLifecycleDependencies(
                    FenixSearchMiddleware.LifecycleDependencies(
                        context = activity,
                        browsingModeManager = activity.browsingModeManager,
                        navController = navController,
                        fenixBrowserUseCases = activity.components.useCases.fenixBrowserUseCases,
                    ),
                )
            } as T

        SearchDialogFragmentStore::class.java ->
            StoreProvider.get(lifecycleOwner) {
                SearchFragmentStore(
                    initialState = createInitialSearchFragmentState(
                        activity = activity,
                        components = components,
                        tabId = browserStore.state.selectedTabId,
                        pastedText = null,
                        searchAccessPoint = MetricsUtils.Source.NONE,
                    ),
                    middleware = listOf(
                        toolbarQueryMapper,
                        searchMiddleware,
                    ),
                )
            } as T

        else -> throw IllegalArgumentException("Unknown type: ${T::class.java}")
    }
}
