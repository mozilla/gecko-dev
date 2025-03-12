/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.setup.store

import mozilla.components.lib.state.Action
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

/**
 * Represents the [State] of the setup checklist feature.
 */
data class SetupChecklistState(
    val collection: SetupChecklistTaskCollection = SetupChecklistTaskCollection.THREE_TASKS,
    val viewState: SetupChecklistViewState = SetupChecklistViewState.FULL,
    val defaultBrowserClicked: Boolean = false,
    val syncClicked: Boolean = false,
    val themeSelectionClicked: Boolean = false,
    val toolbarSelectionClicked: Boolean = false,
    val extensionsClicked: Boolean = false,
    val addSearchWidgetClicked: Boolean = false,
) : State

/**
 * Defines the [Action]s that can be performed in the [SetupChecklistStore].
 */
sealed interface SetupChecklistAction : Action {
    /**
     * When the store is initialized.
     */
    data object Init : SetupChecklistAction

    /**
     * When the setup checklist is closed.
     */
    data object Closed : SetupChecklistAction

    /**
     * When the default browser task in the setup checklist is clicked.
     */
    data object DefaultBrowserClicked : SetupChecklistAction

    /**
     * When the sync task in the setup checklist is clicked.
     */
    data object SyncClicked : SetupChecklistAction

    /**
     * When the theme selection task in the setup checklist is clicked.
     */
    data object ThemeSelectionClicked : SetupChecklistAction

    /**
     * When the toolbar selection task in the setup checklist is clicked.
     */
    data object ToolbarSelectionClicked : SetupChecklistAction

    /**
     * When the extensions task in the setup checklist is clicked.
     */
    data object ExtensionsClicked : SetupChecklistAction

    /**
     * When the add search widget task in the setup checklist is clicked.
     */
    data object AddSearchWidgetClicked : SetupChecklistAction

    /**
     * When the view state is changed.
     */
    data class ViewState(val section: SetupChecklistViewState) : SetupChecklistAction
}

/**
 * Which collection of tasks to show in the setup checklist.
 */
enum class SetupChecklistTaskCollection {
    THREE_TASKS,
    SIX_TASKS,
}

/**
 * The core view state of the setup checklist.
 */
enum class SetupChecklistViewState {
    // The view is minimized. No tasks are visible; only the title and progress indicator are shown.
    MINIMIZED,

    // The full view is shown. The title, progress indicator, and parent tasks are visible,
    // but child tasks are hidden.
    FULL,

    // Same as FULL, but the selected parent task’s child tasks are also visible.
    EXPANDED_FUNDAMENTALS,
    EXPANDED_CUSTOMIZATION,
    EXPANDED_EXTRAS,
}

private fun reducer(
    state: SetupChecklistState,
    action: SetupChecklistAction,
): SetupChecklistState = when (action) {
    is SetupChecklistAction.Init, SetupChecklistAction.Closed -> state

    is SetupChecklistAction.ViewState -> state.copy(viewState = action.section)

    is SetupChecklistAction.DefaultBrowserClicked -> state.copy(defaultBrowserClicked = true)
    is SetupChecklistAction.SyncClicked -> state.copy(syncClicked = true)

    is SetupChecklistAction.ThemeSelectionClicked -> state.copy(themeSelectionClicked = true)
    is SetupChecklistAction.ToolbarSelectionClicked -> state.copy(toolbarSelectionClicked = true)

    is SetupChecklistAction.ExtensionsClicked -> state.copy(extensionsClicked = true)
    is SetupChecklistAction.AddSearchWidgetClicked -> state.copy(addSearchWidgetClicked = true)
}

/**
 * [Store] that holds the [SetupChecklistState] for the setup checklist view and reduces
 * [SetupChecklistAction]s dispatched to the store.
 */
class SetupChecklistStore(
    middleware: List<Middleware<SetupChecklistState, SetupChecklistAction>> = emptyList(),
) : Store<SetupChecklistState, SetupChecklistAction>(
    initialState = SetupChecklistState(),
    reducer = ::reducer,
    middleware = middleware,
) {
    init {
        dispatch(SetupChecklistAction.Init)
    }
}
