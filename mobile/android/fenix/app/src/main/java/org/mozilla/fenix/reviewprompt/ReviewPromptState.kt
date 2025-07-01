/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.reviewprompt

/**
 * Results of the review prompt eligibility check.
 */
sealed interface ReviewPromptState {
    /**
     * The eligibility check hasn't run yet.
     */
    data object Unknown : ReviewPromptState

    /**
     * No triggers were satisfied.
     */
    data object NotEligible : ReviewPromptState

    /**
     * At least one trigger was satisfied and we want to show a prompt of the given [type].
     */
    data class Eligible(val type: Type) : ReviewPromptState {
        /**
         * Types of review prompts we can show.
         */
        enum class Type { PlayStore, Custom }
    }
}
