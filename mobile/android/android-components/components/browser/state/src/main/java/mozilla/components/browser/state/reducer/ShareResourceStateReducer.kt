/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.ShareResourceAction
import mozilla.components.browser.state.state.BrowserState

internal object ShareResourceStateReducer {
    fun reduce(state: BrowserState, action: ShareResourceAction): BrowserState {
        return when (action) {
            is ShareResourceAction.AddShareAction -> updateContentState(state, action.tabId) {
                it.copy(share = action.resource)
            }
            is ShareResourceAction.ConsumeShareAction -> updateContentState(state, action.tabId) {
                it.copy(share = null)
            }
        }
    }
}
