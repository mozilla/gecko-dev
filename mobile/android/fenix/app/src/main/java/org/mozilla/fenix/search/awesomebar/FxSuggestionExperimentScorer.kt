/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.awesomebar

import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.feature.awesomebar.provider.SEARCH_TERMS_MAXIMUM_ALLOWED_SUGGESTIONS_LIMIT
import mozilla.components.feature.awesomebar.provider.SearchSuggestionProvider
import mozilla.components.feature.fxsuggest.FxSuggestSuggestionProvider
import org.mozilla.fenix.components.Core.Companion.METADATA_SHORTCUT_SUGGESTION_LIMIT

/**
 * A scorer implementation used specifically for [FxSuggestSuggestionProvider] experiment.
 *
 * The score is set to guarantee the suggestions are scored lower than that of [SearchSuggestionProvider],
 * which is intended to have the lowest priority among the search providers. This also guarantees that the suggestions
 * will be put at the top of its group.
 */
class FxSuggestionExperimentScorer : AwesomeBar.SuggestionProvider.Scorer {
    private val experimentScore =
        Int.MAX_VALUE - SEARCH_TERMS_MAXIMUM_ALLOWED_SUGGESTIONS_LIMIT - METADATA_SHORTCUT_SUGGESTION_LIMIT - 3

    override fun score(suggestions: List<AwesomeBar.Suggestion>): List<AwesomeBar.Suggestion> {
        return suggestions.map { suggestion ->
            suggestion.copy(score = experimentScore)
        }
    }
}
