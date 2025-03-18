/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.setup.store

import mozilla.components.lib.state.Action
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import org.mozilla.fenix.checklist.ChecklistItem

/**
 * Represents the [State] of the setup checklist feature.
 */
data class SetupChecklistState(
    val checklistItems: List<ChecklistItem> = emptyList(),
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
     * When a setup checklist item is clicked.
     */
    data class ChecklistItemClicked(val item: ChecklistItem) : SetupChecklistAction

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
    is SetupChecklistAction.ChecklistItemClicked -> state.copy(
        checklistItems = state.checklistItems.map { item ->
            when {
                // If the clicked item is a group, we change the expanded state.
                item == action.item && item is ChecklistItem.Group -> item.copy(isExpanded = !item.isExpanded)
                // If the clicked item is a task, we change the completed state.
                item == action.item && item is ChecklistItem.Task -> item.copy(isCompleted = !item.isCompleted)
                // If the clicked item is a task and we found a group, check if we need to update
                // any task within the group.
                action.item is ChecklistItem.Task && item is ChecklistItem.Group -> item.copy(
                    tasks = item.tasks.map { task ->
                        if (task == action.item) task.copy(isCompleted = !task.isCompleted) else task
                    },
                )
                else -> item
            }
        },
    )
}

/**
 * [Store] that holds the [SetupChecklistState] for the setup checklist view and reduces
 * [SetupChecklistAction]s dispatched to the store.
 */
class SetupChecklistStore(
    initialState: SetupChecklistState = SetupChecklistState(),
    middleware: List<Middleware<SetupChecklistState, SetupChecklistAction>> = emptyList(),
) : Store<SetupChecklistState, SetupChecklistAction>(
    initialState = initialState,
    reducer = ::reducer,
    middleware = middleware,
) {
    init {
        dispatch(SetupChecklistAction.Init)
    }
}
