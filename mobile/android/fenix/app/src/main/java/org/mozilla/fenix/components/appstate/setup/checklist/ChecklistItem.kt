/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.appstate.setup.checklist

import androidx.annotation.DrawableRes

/**
 * A sealed class representing an item in the setup checklist.
 *
 * @property title the title of the checklist item.
 */
sealed class ChecklistItem(open val title: String) {

    /**
     * A data class representing an individual task in the checklist.
     *
     * @property type The type of the task.
     * @property title the task's title.
     * @property icon The icon resource to be displayed on the right of the task.
     * @property isCompleted whether the task has been completed.
     */
    data class Task(
        val type: Type,
        override val title: String,
        @DrawableRes val icon: Int,
        val isCompleted: Boolean,
    ) : ChecklistItem(title) {

        /**
         * A check list task type.
         */
        enum class Type(val telemetryName: String) {
            SET_AS_DEFAULT("default-browser"),
            SIGN_IN("sign-in"),
            SELECT_THEME("theme-selection"),
            CHANGE_TOOLBAR_PLACEMENT("toolbar-selection"),
            INSTALL_SEARCH_WIDGET("search-widget"),
            EXPLORE_EXTENSION("extensions"),
        }
    }

    /**
     * A data class representing a group of tasks in the checklist.
     *
     * @property title the group's title.
     * @property tasks the list of tasks that belong to this group.
     * @property isExpanded whether the group is currently expanded.
     */
    data class Group(
        override val title: String,
        val tasks: List<Task>,
        val isExpanded: Boolean,
    ) : ChecklistItem(title) {
        val progress: Progress = tasks.getTaskProgress()
    }
}

/**
 * A data class representing the task progress.
 *
 * @property totalTasks The total number of tasks in the checklist.
 * @property completedTasks The number of completed tasks.
 */
data class Progress(
    var totalTasks: Int = 0,
    var completedTasks: Int = 0,
) {

    /**
     * Checks if all tasks in the checklist are completed.
     */
    fun allTasksCompleted() = totalTasks == completedTasks
}

/**
 * Calculates the completion progress of a set of [ChecklistItem].
 */
fun List<ChecklistItem>.getTaskProgress(): Progress {
    val tasks = flatMap { item ->
        when (item) {
            is ChecklistItem.Task -> listOf(item)
            is ChecklistItem.Group -> item.tasks
        }
    }

    return Progress(
        totalTasks = tasks.size,
        completedTasks = tasks.count { it.isCompleted },
    )
}
