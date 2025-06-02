/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.reviewprompt

import org.mozilla.fenix.reviewprompt.ui.CustomReviewPrompt

/**
 * Available steps the [CustomReviewPrompt] can be showing.
 */
enum class CustomReviewPromptState {
    /** Initial state with positive and negative buttons to rate the experience. */
    PrePrompt,

    /** Positive state with a button to leave a Play Store rating. */
    Rate,

    /** Negative state with a button to leave feedback. */
    Feedback,
}
