/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import android.content.res.Resources
import androidx.annotation.VisibleForTesting
import androidx.core.graphics.drawable.toDrawable
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.navigation.NavController
import mozilla.components.browser.state.action.AwesomeBarAction
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.search.SearchEngine.Type.APPLICATION
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.compose.browser.toolbar.BrowserToolbar
import mozilla.components.compose.browser.toolbar.concept.Action.SearchSelectorAction
import mozilla.components.compose.browser.toolbar.store.BrowserEditToolbarAction.SearchActionsStartUpdated
import mozilla.components.compose.browser.toolbar.store.BrowserEditToolbarAction.UpdateEditText
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
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.UnifiedSearch
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction.SearchEngineSelected
import org.mozilla.fenix.components.appstate.AppAction.UpdateSearchBeingActiveState
import org.mozilla.fenix.search.SearchSelectorEvents.SearchSelectorClicked
import org.mozilla.fenix.search.SearchSelectorEvents.SearchSelectorItemClicked
import org.mozilla.fenix.search.SearchSelectorEvents.SearchSettingsItemClicked
import org.mozilla.fenix.search.ext.searchEngineShortcuts
import mozilla.components.compose.browser.toolbar.concept.Action.SearchSelectorAction.ContentDescription.StringContentDescription as SearchSelectorDescription
import mozilla.components.compose.browser.toolbar.concept.Action.SearchSelectorAction.Icon.DrawableIcon as SearchSelectorIcon
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton.ContentDescription.StringContentDescription as MenuItemStringDescription
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton.ContentDescription.StringResContentDescription as MenuItemDescriptionRes
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton.Icon.DrawableIcon as MenuItemIcon
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton.Icon.DrawableResIcon as MenuItemIconRes
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton.Text.StringResText as MenuItemStringResText
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton.Text.StringText as MenuItemStringText

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
 */
class BrowserToolbarSearchMiddleware(
    private val appStore: AppStore,
    private val browserStore: BrowserStore,
) : Middleware<BrowserToolbarState, BrowserToolbarAction>, ViewModel() {
    private lateinit var toolbarStore: BrowserToolbarStore
    private lateinit var dependencies: LifecycleDependencies

    /**
     * Updates the [LifecycleDependencies] of this middleware.
     *
     * @param dependencies The new [LifecycleDependencies].
     */
    fun updateLifecycleDependencies(dependencies: LifecycleDependencies) {
        this.dependencies = dependencies
    }

    override fun invoke(
        context: MiddlewareContext<BrowserToolbarState, BrowserToolbarAction>,
        next: (BrowserToolbarAction) -> Unit,
        action: BrowserToolbarAction,
    ) {
        when (action) {
            is Init -> {
                toolbarStore = context.store as BrowserToolbarStore
            }

            is ToggleEditMode -> {
                if (action.editMode == true) {
                    val searchState = browserStore.state.search
                    updateSearchSelectorMenu(
                        searchState.selectedOrDefaultSearchEngine,
                        searchState.searchEngineShortcuts,
                    )
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
                appStore.dispatch(SearchEngineSelected(action.searchEngine))
                updateSearchSelectorMenu(action.searchEngine, browserStore.state.search.searchEngineShortcuts)
            }

            else -> {
                // no-op.
            }
        }

        next(action)
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
         */
        fun viewModelFactory(
            appStore: AppStore,
            browserStore: BrowserStore,
        ): ViewModelProvider.Factory = object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T =
                BrowserToolbarSearchMiddleware(appStore, browserStore) as? T
                    ?: throw IllegalArgumentException("Unknown ViewModel class")
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
