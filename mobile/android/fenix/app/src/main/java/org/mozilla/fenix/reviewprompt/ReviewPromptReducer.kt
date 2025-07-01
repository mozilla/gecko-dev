/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.reviewprompt

import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction.ReviewPromptAction
import org.mozilla.fenix.components.appstate.AppAction.ReviewPromptAction.CheckIfEligibleForReviewPrompt
import org.mozilla.fenix.components.appstate.AppAction.ReviewPromptAction.DoNotShowReviewPrompt
import org.mozilla.fenix.components.appstate.AppAction.ReviewPromptAction.ReviewPromptShown
import org.mozilla.fenix.components.appstate.AppAction.ReviewPromptAction.ShowCustomReviewPrompt
import org.mozilla.fenix.components.appstate.AppAction.ReviewPromptAction.ShowPlayStorePrompt
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.reviewprompt.ReviewPromptState.Eligible
import org.mozilla.fenix.reviewprompt.ReviewPromptState.Eligible.Type
import org.mozilla.fenix.reviewprompt.ReviewPromptState.NotEligible

/**
 * [AppStore] reducer of [ReviewPromptAction]s.
 */
internal object ReviewPromptReducer {
    fun reduce(state: AppState, action: ReviewPromptAction): AppState {
        return when (action) {
            ShowPlayStorePrompt -> state.copy(reviewPrompt = Eligible(Type.PlayStore))
            ShowCustomReviewPrompt -> state.copy(reviewPrompt = Eligible(Type.Custom))
            DoNotShowReviewPrompt, ReviewPromptShown -> state.copy(reviewPrompt = NotEligible)
            CheckIfEligibleForReviewPrompt -> state
        }
    }
}
