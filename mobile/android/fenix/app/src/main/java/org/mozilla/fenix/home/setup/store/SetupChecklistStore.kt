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
            // A local variable to clearly distinguish between
            // the item in the list and the item from the action.
            val clickedItem = action.item

            when (item) {
                is ChecklistItem.Group -> {
                    when (clickedItem) {
                        // Given the clicked item is a group, when we find a group in the list,
                        // we change its state. Only one group can be expanded at a time.
                        is ChecklistItem.Group -> {
                            if (item == clickedItem) {
                                item.copy(isExpanded = !clickedItem.isExpanded)
                            } else {
                                item.copy(
                                    isExpanded = false,
                                )
                            }
                        }

                        // Given the clicked item is a task, when we find a group in the list,
                        // we check the group tasks for the clicked one.
                        is ChecklistItem.Task -> item.copy(
                            tasks = item.tasks.map { task ->
                                if (task == clickedItem) {
                                    task.copy(isCompleted = !clickedItem.isCompleted)
                                } else {
                                    task
                                }
                            },
                        )
                    }
                }

                // Given the clicked item is a task, when we find a task in the list,
                // we change its completed state.
                is ChecklistItem.Task -> if (item == clickedItem) {
                    item.copy(isCompleted = !(clickedItem as ChecklistItem.Task).isCompleted)
                } else {
                    item
                }
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
