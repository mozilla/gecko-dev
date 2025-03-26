/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.setup.store

import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.components.appstate.setup.checklist.ChecklistItem
import org.mozilla.fenix.components.appstate.setup.checklist.getTaskProgress

class ChecklistItemTest {

    @Test
    fun `GIVEN an empty list WHEN getTaskProgress is invoked THEN total and completed values should be zero`() {
        val checklist = emptyList<ChecklistItem>()

        val progress = checklist.getTaskProgress()

        assertEquals(0, progress.totalTasks)
        assertEquals(0, progress.completedTasks)
    }

    @Test
    fun `GIVEN a single completed task WHEN getTaskProgress is invoked THEN total and completed values should be equal to 1`() {
        val checklist = listOf(buildTask("Task 1", isCompleted = true))

        val progress = checklist.getTaskProgress()

        assertEquals(1, progress.totalTasks)
        assertEquals(1, progress.completedTasks)
    }

    @Test
    fun `GIVEN a checklist with multiple tasks and a single completed task WHEN getTaskProgress is invoked THEN total and completed values should be equal to 1 still`() {
        val checklist = listOf(
            buildTask("Task 1", isCompleted = true),
            buildTask("Task 2", isCompleted = false),
            buildTask("Task 3", isCompleted = false),
        )

        val progress = checklist.getTaskProgress()

        assertEquals(3, progress.totalTasks)
        assertEquals(1, progress.completedTasks)
    }

    @Test
    fun `GIVEN a checklist with a group containing tasks WHEN getTaskProgress is invoked THEN tasks within the group are calculated correctly`() {
        val checklist = listOf(
            ChecklistItem.Group(
                title = "Group 1",
                tasks = listOf(
                    buildTask("Task 1", isCompleted = true),
                    buildTask("Task 2", isCompleted = false),
                ),
                isExpanded = false,
            ),
        )

        val progress = checklist.getTaskProgress()

        assertEquals(2, progress.totalTasks)
        assertEquals(1, progress.completedTasks)
    }

    @Test
    fun `GIVEN a checklist with multiple groups WHEN getTaskProgress is called THEN tasks within the groups are calculated correctly`() {
        val checklist = listOf(
            ChecklistItem.Group(
                title = "Group 1",
                tasks = listOf(
                    buildTask("Task 1", isCompleted = true),
                    buildTask("Task 2", isCompleted = false),
                ),
                isExpanded = false,
            ),
            ChecklistItem.Group(
                title = "Group 2",
                tasks = listOf(
                    buildTask("Task 3", isCompleted = true),
                    buildTask("Task 4", isCompleted = false),
                    buildTask("Task 5", isCompleted = false),
                ),
                isExpanded = false,
            ),
        )

        val progress = checklist.getTaskProgress()

        assertEquals(5, progress.totalTasks)
        assertEquals(2, progress.completedTasks)
    }

    @Test
    fun `GIVEN a checklist with both standalone tasks and a group WHEN getTaskProgress is called THEN counts all tasks correctly`() {
        val checklist = listOf(
            buildTask("Task 1", isCompleted = true),
            ChecklistItem.Group(
                title = "Group 1",
                tasks = listOf(
                    buildTask("Task 2", isCompleted = false),
                    buildTask("Task 3", isCompleted = true),
                ),
                isExpanded = false,
            ),
        )

        val progress = checklist.getTaskProgress()

        assertEquals(3, progress.totalTasks)
        assertEquals(2, progress.completedTasks)
    }

    private fun buildTask(
        title: String,
        isCompleted: Boolean,
    ) = ChecklistItem.Task(
        type = ChecklistItem.Task.Type.SIGN_IN,
        title = title,
        icon = 0,
        isCompleted = isCompleted,
    )
}
