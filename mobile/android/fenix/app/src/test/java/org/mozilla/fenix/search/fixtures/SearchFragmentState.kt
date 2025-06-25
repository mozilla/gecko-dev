/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.fixtures

import org.mozilla.fenix.components.metrics.MetricsUtils
import org.mozilla.fenix.search.SearchEngineSource
import org.mozilla.fenix.search.SearchFragmentState

/**
 * Prebuilt empty [SearchFragmentState].
 */
val EMPTY_SEARCH_FRAGMENT_STATE = SearchFragmentState(
    query = "",
    url = "",
    searchTerms = "",
    searchEngineSource = SearchEngineSource.None,
    defaultEngine = null,
    searchSuggestionsProviders = emptyList(),
    searchSuggestionsOrientedAtBottom = false,
    shouldShowSearchSuggestions = false,
    showSearchSuggestions = false,
    showSearchSuggestionsHint = false,
    showSearchShortcuts = false,
    areShortcutsAvailable = false,
    showSearchShortcutsSetting = false,
    showClipboardSuggestions = false,
    showSearchTermHistory = false,
    showHistorySuggestionsForCurrentEngine = false,
    showAllHistorySuggestions = false,
    showBookmarksSuggestionsForCurrentEngine = false,
    showAllBookmarkSuggestions = false,
    showSyncedTabsSuggestionsForCurrentEngine = false,
    showAllSyncedTabsSuggestions = false,
    showSessionSuggestionsForCurrentEngine = false,
    showAllSessionSuggestions = false,
    showSponsoredSuggestions = false,
    showNonSponsoredSuggestions = false,
    showTrendingSearches = false,
    showRecentSearches = false,
    showShortcutsSuggestions = false,
    showQrButton = false,
    tabId = null,
    pastedText = null,
    searchAccessPoint = MetricsUtils.Source.NONE,
    clipboardHasUrl = false,
)
