/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.DefaultDesktopModeAction
import mozilla.components.browser.state.state.BrowserState

/**
 * An [DefaultDesktopModeAction] reducer that updates [BrowserState.desktopMode].
 */
internal object DesktopModeReducer {
    fun reduce(state: BrowserState, action: DefaultDesktopModeAction): BrowserState {
        return when (action) {
            is DefaultDesktopModeAction.ToggleDesktopMode -> state.copy(desktopMode = !state.desktopMode)
            is DefaultDesktopModeAction.DesktopModeUpdated -> state.copy(desktopMode = action.newValue)
        }
    }
}
