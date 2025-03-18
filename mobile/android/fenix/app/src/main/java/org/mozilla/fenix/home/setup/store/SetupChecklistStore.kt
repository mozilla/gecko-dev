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
     * When a setup checklist item is clicked.
     */
    data class ChecklistItemClicked(val item: ChecklistItem) : SetupChecklistAction
}

private fun reducer(
    state: SetupChecklistState,
    action: SetupChecklistAction,
): SetupChecklistState = when (action) {
    is SetupChecklistAction.Init, SetupChecklistAction.Closed -> state
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
