/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.awesomebar

import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.search.SearchFragmentState

/**
 * Configures what search suggestions to show in the awesome bar.
 *
 * @param activity [HomeActivity] used for accessing needing dependencies.
 * @param interactor [AwesomeBarInteractor] used for handling user interactions with the search suggestions.
 * @param view [AwesomeBarWrapper] used for displaying the search suggestions.
 * @param includeSelectedTab Whether or not to include the current tab in search suggestions.
 */
@Suppress("OutdatedDocumentation")
class AwesomeBarView(
    activity: HomeActivity,
    private val interactor: AwesomeBarInteractor,
    val view: AwesomeBarWrapper,
    includeSelectedTab: Boolean,
) {
    private val suggestionsProvidersBuilder by lazy(LazyThreadSafetyMode.NONE) {
        SearchSuggestionsProvidersBuilder(
            activity = activity,
            includeSelectedTab = includeSelectedTab,
            loadUrlUseCase = AwesomeBarLoadUrlUseCase(interactor),
            searchUseCase = AwesomeBarSearchUseCase(interactor),
            selectTabUseCase = AwesomeBarSelectTabUseCase(interactor),
            onSearchEngineShortcutSelected = interactor::onSearchShortcutEngineSelected,
            onSearchEngineSuggestionSelected = interactor::onSearchEngineSuggestionSelected,
            onSearchEngineSettingsClicked = interactor::onClickSearchEngineSettings,
        )
    }

    /**
     * Updates the search suggestions based on the search query and other properties
     * from [SearchFragmentState] changes.
     *
     * @param state [SearchFragmentState] containing the current search state based on which
     * new search suggestions will be provided.
     */
    fun update(state: SearchFragmentState) {
        // Do not make suggestions based on user's current URL unless it's a search shortcut
        if (state.query.isNotEmpty() && state.query == state.url && !state.showSearchShortcuts) {
            return
        }

        view.onInputChanged(state.query)
    }

    /**
     * Updates the types of search suggestions to show based on the current [SearchFragmentState].
     *
     * @param state [SearchFragmentState] containing the current search state based on which different
     * suggestions providers will be made available for offering search suggestions.
     */
    fun updateSuggestionProvidersVisibility(
        state: SearchFragmentState,
    ) {
        view.removeAllProviders()

        if (state.showSearchShortcuts) {
            view.addProviders(suggestionsProvidersBuilder.shortcutsEnginePickerProvider)
            return
        }

        for (provider in suggestionsProvidersBuilder.getProvidersToAdd(state.toSearchProviderState())) {
            view.addProviders(provider)
        }
    }
}
