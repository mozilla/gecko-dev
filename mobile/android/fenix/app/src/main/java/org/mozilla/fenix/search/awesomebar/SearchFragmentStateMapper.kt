/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.awesomebar

import org.mozilla.fenix.search.SearchFragmentState
import org.mozilla.fenix.search.awesomebar.SearchSuggestionsProvidersBuilder.SearchProviderState

/**
 * Map [SearchFragmentState] to [SearchProviderState] as a subset of properties only specific to search.
 */
fun SearchFragmentState.toSearchProviderState() = SearchProviderState(
    showSearchShortcuts = showSearchShortcuts,
    showSearchTermHistory = showSearchTermHistory,
    showHistorySuggestionsForCurrentEngine = showHistorySuggestionsForCurrentEngine,
    showAllHistorySuggestions = showAllHistorySuggestions,
    showBookmarksSuggestionsForCurrentEngine = showBookmarksSuggestionsForCurrentEngine,
    showAllBookmarkSuggestions = showAllBookmarkSuggestions,
    showSearchSuggestions = showSearchSuggestions,
    showSyncedTabsSuggestionsForCurrentEngine = showSyncedTabsSuggestionsForCurrentEngine,
    showAllSyncedTabsSuggestions = showAllSyncedTabsSuggestions,
    showSessionSuggestionsForCurrentEngine = showSessionSuggestionsForCurrentEngine,
    showAllSessionSuggestions = showAllSessionSuggestions,
    showSponsoredSuggestions = showSponsoredSuggestions,
    showNonSponsoredSuggestions = showNonSponsoredSuggestions,
    showTrendingSearches = showTrendingSearches,
    showRecentSearches = showRecentSearches,
    showShortcutsSuggestions = showShortcutsSuggestions,
    searchEngineSource = searchEngineSource,
)
