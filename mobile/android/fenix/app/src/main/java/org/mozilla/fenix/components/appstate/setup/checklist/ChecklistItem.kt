/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.appstate.setup.checklist

import androidx.annotation.DrawableRes
import androidx.annotation.StringRes
import org.mozilla.fenix.R
import org.mozilla.fenix.nimbus.SetupChecklistType

/**
 * A sealed class representing an item in the setup checklist.
 *
 * @property title the string resource for the checklist item title.
 */
sealed class ChecklistItem(@StringRes open val title: Int) {
    /**
     * A data class representing an individual task in the checklist.
     *
     * @property type the type of the task.
     * @property title the string resource of the task's title.
     * @property icon the icon resource to be displayed on the right of the task.
     * @property isCompleted whether the task has been completed.
     */
    data class Task(
        val type: Type,
        @StringRes override val title: Int,
        @DrawableRes val icon: Int,
        val isCompleted: Boolean = false,
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
     * @property title the string resource of the group's title.
     * @property tasks the list of tasks that belong to this group.
     * @property isExpanded whether the group is currently expanded.
     */
    data class Group(
        @StringRes override val title: Int,
        val tasks: List<Task>,
        val isExpanded: Boolean = false,
    ) : ChecklistItem(title) {
        val progress: Progress = tasks.getTaskProgress()
    }
}

/**
 * A data class representing the task progress.
 *
 * @property totalTasks the total number of tasks in the checklist.
 * @property completedTasks the number of completed tasks.
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

/**
 * Gets the checklist items for the given [SetupChecklistType].
 */
fun getSetupChecklistCollection(collection: SetupChecklistType) = when (collection) {
    SetupChecklistType.COLLECTION_1 -> createTasksCollection()
    SetupChecklistType.COLLECTION_2 -> createGroupsCollection()
}

private fun createTasksCollection() =
    listOf(defaultBrowserTask(), exploreExtensionTask(), signInTask())

private fun defaultBrowserTask() = ChecklistItem.Task(
    type = ChecklistItem.Task.Type.SET_AS_DEFAULT,
    title = R.string.setup_checklist_task_default_browser,
    icon = R.drawable.mozac_ic_web_extension_default_icon,
)

private fun exploreExtensionTask() = ChecklistItem.Task(
    type = ChecklistItem.Task.Type.EXPLORE_EXTENSION,
    title = R.string.setup_checklist_task_explore_extensions,
    icon = R.drawable.mozac_ic_web_extension_default_icon,
)

private fun signInTask() = ChecklistItem.Task(
    type = ChecklistItem.Task.Type.SIGN_IN,
    title = R.string.setup_checklist_task_account_sync,
    icon = R.drawable.mozac_ic_web_extension_default_icon,
)

private fun createGroupsCollection() =
    listOf(essentialsGroup(), customizeGroup(), helpfulToolsGroup())

private fun essentialsGroup() = ChecklistItem.Group(
    title = R.string.setup_checklist_group_essentials,
    tasks = listOf(defaultBrowserTask(), signInTask()),
)

private fun customizeGroup() = ChecklistItem.Group(
    title = R.string.setup_checklist_group_customize,
    tasks = listOf(
        ChecklistItem.Task(
            type = ChecklistItem.Task.Type.SELECT_THEME,
            title = R.string.setup_checklist_task_theme_selection,
            icon = R.drawable.mozac_ic_web_extension_default_icon,
        ),
        ChecklistItem.Task(
            type = ChecklistItem.Task.Type.CHANGE_TOOLBAR_PLACEMENT,
            title = R.string.setup_checklist_task_theme_selection,
            icon = R.drawable.mozac_ic_web_extension_default_icon,
        ),
    ),
)

private fun helpfulToolsGroup() = ChecklistItem.Group(
    title = R.string.setup_checklist_group_helpful_tools,
    tasks = listOf(
        ChecklistItem.Task(
            type = ChecklistItem.Task.Type.INSTALL_SEARCH_WIDGET,
            title = R.string.setup_checklist_task_theme_selection,
            icon = R.drawable.mozac_ic_web_extension_default_icon,
        ),
        exploreExtensionTask(),
    ),
)
