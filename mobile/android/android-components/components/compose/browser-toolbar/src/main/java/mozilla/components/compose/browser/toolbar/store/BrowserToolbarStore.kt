/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar.store

import mozilla.components.lib.state.UiStore

/**
 * [UiStore] for maintaining the state of the browser toolbar.
 */
class BrowserToolbarStore(
    initialState: BrowserToolbarState = BrowserToolbarState(),
) : UiStore<BrowserToolbarState, BrowserToolbarAction>(
    initialState = initialState,
    reducer = ::reduce,
)

private fun reduce(state: BrowserToolbarState, action: BrowserToolbarAction): BrowserToolbarState {
    return when (action) {
        is BrowserToolbarAction.ToggleEditMode -> state.copy(
            editMode = action.editMode,
            editState = state.editState.copy(
                editText = if (action.editMode) null else state.editState.editText,
            ),
        )

        is BrowserEditToolbarAction.UpdateEditText -> state.copy(
            editState = state.editState.copy(
                editText = action.text,
            ),
        )

        is BrowserEditToolbarAction.AddEditActionStart -> state.copy(
            editState = state.editState.copy(
                editActionsStart = state.editState.editActionsStart + action.action,
            ),
        )

        is BrowserEditToolbarAction.AddEditActionEnd -> state.copy(
            editState = state.editState.copy(
                editActionsEnd = state.editState.editActionsEnd + action.action,
            ),
        )
    }
}
