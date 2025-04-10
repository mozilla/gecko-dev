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
                val updatedItems = it.checklistItems.map { existingItem ->
                    // A local variable to clearly distinguish between
                    // the item in the list and the item from the action.
                    val clickedItem = action.item

                    when (existingItem) {
                        is ChecklistItem.Group -> {
                            when (clickedItem) {
                                // Given the clicked item is a group, when we find a group in the list,
                                // we change its state. Only one group can be expanded at a time.
                                is ChecklistItem.Group -> {
                                    if (existingItem == clickedItem) {
                                        existingItem.copy(isExpanded = !clickedItem.isExpanded)
                                    } else {
                                        existingItem.copy(isExpanded = false)
                                    }
                                }

                                // Given the clicked item is a task and not already completed, when
                                // we find a group in the list, we check the group tasks for the
                                // clicked one.
                                is ChecklistItem.Task -> existingItem.copy(
                                    tasks = existingItem.tasks.map { existingTask ->
                                        if (!existingTask.isCompleted && existingTask == clickedItem) {
                                            existingTask.copy(isCompleted = true)
                                        } else {
                                            existingTask
                                        }
                                    },
                                )
                            }
                        }

                        // Given the clicked item is a task and not already completed, when we find
                        // a task in the list, we change its completed state.
                        is ChecklistItem.Task -> if (!existingItem.isCompleted && existingItem == clickedItem) {
                            existingItem.copy(isCompleted = true)
                        } else {
                            existingItem
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
