/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar.store

import mozilla.components.compose.browser.toolbar.store.BrowserDisplayToolbarAction.BrowserActionsEndUpdated
import mozilla.components.compose.browser.toolbar.store.BrowserDisplayToolbarAction.BrowserActionsStartUpdated
import mozilla.components.compose.browser.toolbar.store.BrowserDisplayToolbarAction.PageActionsEndUpdated
import mozilla.components.compose.browser.toolbar.store.BrowserDisplayToolbarAction.PageActionsStartUpdated
import mozilla.components.compose.browser.toolbar.store.BrowserDisplayToolbarAction.PageOriginUpdated
import mozilla.components.compose.browser.toolbar.store.BrowserEditToolbarAction.UrlSuggestionAutocompleted
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarEvent
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.UiStore

/**
 * [UiStore] for maintaining the state of the browser toolbar.
 */
open class BrowserToolbarStore(
    initialState: BrowserToolbarState = BrowserToolbarState(),
    middleware: List<Middleware<BrowserToolbarState, BrowserToolbarAction>> = emptyList(),
) : UiStore<BrowserToolbarState, BrowserToolbarAction>(
    initialState = initialState,
    reducer = ::reduce,
    middleware = middleware,
) {
    init {
        // Allow integrators intercept and update the initial state.
        dispatch(
            BrowserToolbarAction.Init(
                mode = initialState.mode,
                displayState = initialState.displayState,
                editState = initialState.editState,
            ),
        )
    }
}

private fun reduce(state: BrowserToolbarState, action: BrowserToolbarAction): BrowserToolbarState {
    return when (action) {
        is BrowserToolbarAction.Init -> BrowserToolbarState(
            mode = action.mode,
            displayState = action.displayState,
            editState = action.editState,
        )

        is BrowserToolbarAction.ToggleEditMode -> state.copy(
            mode = if (action.editMode) Mode.EDIT else Mode.DISPLAY,
            editState = state.editState.copy(
                query = if (action.editMode) state.editState.query else "",
            ),
        )

        is BrowserToolbarAction.CommitUrl -> state

        is BrowserActionsStartUpdated -> state.copy(
            displayState = state.displayState.copy(
                browserActionsStart = action.actions,
            ),
        )

        is PageActionsStartUpdated -> state.copy(
            displayState = state.displayState.copy(
                pageActionsStart = action.actions,
            ),
        )

        is PageOriginUpdated -> state.copy(
            displayState = state.displayState.copy(
                pageOrigin = action.pageOrigin,
            ),
        )

        is PageActionsEndUpdated -> state.copy(
            displayState = state.displayState.copy(
                pageActionsEnd = action.actions,
            ),
        )

        is BrowserActionsEndUpdated -> state.copy(
            displayState = state.displayState.copy(
                browserActionsEnd = action.actions,
            ),
        )

        is BrowserEditToolbarAction.SearchQueryUpdated -> state.copy(
            editState = state.editState.copy(
                query = action.query,
                showQueryAsPreselected = action.showAsPreselected,
            ),
        )

        is BrowserEditToolbarAction.AutocompleteProvidersUpdated -> state.copy(
            editState = state.editState.copy(
                autocompleteProviders = action.autocompleteProviders,
            ),
        )

        is BrowserEditToolbarAction.SearchActionsStartUpdated -> state.copy(
            editState = state.editState.copy(
                editActionsStart = action.actions,
            ),
        )

        is BrowserEditToolbarAction.SearchActionsEndUpdated -> state.copy(
            editState = state.editState.copy(
                editActionsEnd = action.actions,
            ),
        )

        is BrowserDisplayToolbarAction.UpdateProgressBarConfig -> state.copy(
            displayState = state.displayState.copy(
                progressBarConfig = action.config,
            ),
        )

        is EnvironmentRehydrated,
        is EnvironmentCleared,
        is UrlSuggestionAutocompleted,
        is BrowserToolbarEvent,
            -> {
            // no-op
            // Expected to be handled in middlewares set by integrators.
            state
        }
    }
}
