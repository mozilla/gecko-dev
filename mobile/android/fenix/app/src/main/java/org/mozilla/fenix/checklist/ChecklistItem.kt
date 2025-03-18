/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.checklist

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
     * @property title the task's title.
     * @property icon The icon resource to be displayed on the right of the task.
     * @property isCompleted whether the task has been completed.
     */
    data class Task(
        override val title: String,
        @DrawableRes val icon: Int,
        val isCompleted: Boolean,
    ) : ChecklistItem(title)

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
    ) : ChecklistItem(title)
}
