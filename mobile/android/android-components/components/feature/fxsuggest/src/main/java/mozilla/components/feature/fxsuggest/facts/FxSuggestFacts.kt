/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.fxsuggest.facts

import mozilla.components.feature.fxsuggest.FxSuggestInteractionInfo
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.collect

/**
 * Facts emitted for telemetry related to the Firefox Suggest feature.
 */
class FxSuggestFacts {
    /**
     * Specific types of telemetry items.
     */
    object Items {
        const val AMP_SUGGESTION_CLICKED = "amp_suggestion_clicked"
        const val AMP_SUGGESTION_IMPRESSED = "amp_suggestion_impressed"
        const val WIKIPEDIA_SUGGESTION_CLICKED = "wikipedia_suggestion_clicked"
        const val WIKIPEDIA_SUGGESTION_IMPRESSED = "wikipedia_suggestion_impressed"
    }

    /**
     * Keys used in the metadata map.
     */
    object MetadataKeys {
        const val INTERACTION_INFO = "interaction_info"
        const val POSITION = "position"
        const val IS_CLICKED = "is_clicked"
        const val ENGAGEMENT_ABANDONED = "engagement_abandoned"
        const val CLIENT_COUNTRY = "client_country"
    }
}

private fun emitFxSuggestFact(
    action: Action,
    item: String,
    value: String? = null,
    metadata: Map<String, Any>? = null,
) {
    Fact(
        Component.FEATURE_FXSUGGEST,
        action,
        item,
        value,
        metadata,
    ).collect()
}

internal fun emitSuggestionClickedFact(
    interactionInfo: FxSuggestInteractionInfo,
    positionInAwesomeBar: Long,
    clientCountry: String,
) {
    emitFxSuggestFact(
        Action.INTERACTION,
        when (interactionInfo) {
            is FxSuggestInteractionInfo.Amp -> FxSuggestFacts.Items.AMP_SUGGESTION_CLICKED
            is FxSuggestInteractionInfo.Wikipedia -> FxSuggestFacts.Items.WIKIPEDIA_SUGGESTION_CLICKED
        },
        metadata = mapOf(
            FxSuggestFacts.MetadataKeys.INTERACTION_INFO to interactionInfo,
            FxSuggestFacts.MetadataKeys.POSITION to positionInAwesomeBar,
            FxSuggestFacts.MetadataKeys.CLIENT_COUNTRY to clientCountry,
        ),
    )
}

internal fun emitSuggestionImpressedFact(
    interactionInfo: FxSuggestInteractionInfo,
    positionInAwesomeBar: Long,
    isClicked: Boolean,
    clientCountry: String,
    engagementAbandoned: Boolean,
) {
    emitFxSuggestFact(
        Action.DISPLAY,
        when (interactionInfo) {
            is FxSuggestInteractionInfo.Amp -> FxSuggestFacts.Items.AMP_SUGGESTION_IMPRESSED
            is FxSuggestInteractionInfo.Wikipedia -> FxSuggestFacts.Items.WIKIPEDIA_SUGGESTION_IMPRESSED
        },
        metadata = mapOf(
            FxSuggestFacts.MetadataKeys.INTERACTION_INFO to interactionInfo,
            FxSuggestFacts.MetadataKeys.POSITION to positionInAwesomeBar,
            FxSuggestFacts.MetadataKeys.IS_CLICKED to isClicked,
            FxSuggestFacts.MetadataKeys.ENGAGEMENT_ABANDONED to engagementAbandoned,
            FxSuggestFacts.MetadataKeys.CLIENT_COUNTRY to clientCountry,
        ),
    )
}
