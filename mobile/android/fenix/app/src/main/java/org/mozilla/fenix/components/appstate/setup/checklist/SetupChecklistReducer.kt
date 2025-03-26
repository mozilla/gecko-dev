/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.appstate.setup.checklist

import org.mozilla.fenix.components.appstate.AppAction.SetupChecklistAction
import org.mozilla.fenix.components.appstate.AppState

/**
 * Helper object which reduces [SetupChecklistAction]s.
 */
internal object SetupChecklistReducer {

    /**
     * Reduces [SetupChecklistAction]s and performs any necessary state mutations.
     *
     * @param state The current snapshot of [AppState].
     * @param action The [SetupChecklistAction] being reduced.
     * @return The resulting [AppState] after the given [action] has been reduced.
     */
    fun reduce(state: AppState, action: SetupChecklistAction): AppState = when (action) {
        SetupChecklistAction.Closed -> state
        is SetupChecklistAction.ChecklistItemClicked -> {
            state.setupChecklistState?.let {
                val updatedItems = it.checklistItems.map { item ->
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
                                        item.copy(isExpanded = false)
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
                }

                state.copy(
                    setupChecklistState = state.setupChecklistState.copy(
                        checklistItems = updatedItems,
                        progress = updatedItems.getTaskProgress(),
                    ),
                )
            } ?: state
        }
    }
}
