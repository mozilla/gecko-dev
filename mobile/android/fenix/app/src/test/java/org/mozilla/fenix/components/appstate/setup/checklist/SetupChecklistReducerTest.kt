/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.appstate.setup.checklist

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppState

class SetupChecklistReducerTest {

    @Test
    fun `GIVEN no setup checklist state WHEN checklist item clicked action THEN the reduced state remains the same`() {
        val task = ChecklistItem.Task(
            type = ChecklistItem.Task.Type.EXPLORE_EXTENSION,
            title = "task",
            icon = R.drawable.ic_addons_extensions,
            isCompleted = false,
        )

        val appState = AppState()
        val reducedState = SetupChecklistReducer.reduce(
            appState,
            AppAction.SetupChecklistAction.ChecklistItemClicked(task),
        )

        assertEquals(appState, reducedState)
    }

    @Test
    fun `WHEN closed action THEN the reduced state remains the same`() {
        val appState = AppState(setupChecklistState = SetupChecklistState())

        val reducedState =
            SetupChecklistReducer.reduce(appState, AppAction.SetupChecklistAction.Closed)

        assertEquals(appState, reducedState)
    }

    @Test
    fun `WHEN a group item is clicked THEN the reduced state has the group expanded and other groups collapsed`() {
        val expandedGroup = ChecklistItem.Group(
            title = "group1",
            tasks = listOf(
                ChecklistItem.Task(
                    type = ChecklistItem.Task.Type.SET_AS_DEFAULT,
                    title = "task1",
                    icon = R.drawable.ic_addons_extensions,
                    isCompleted = false,
                ),
            ),
            isExpanded = true,
        )
        val collapsedGroup = ChecklistItem.Group(
            title = "group2",
            tasks = listOf(
                ChecklistItem.Task(
                    type = ChecklistItem.Task.Type.INSTALL_SEARCH_WIDGET,
                    title = "task2",
                    icon = R.drawable.ic_addons_extensions,
                    isCompleted = false,
                ),
            ),
            isExpanded = false,
        )

        val appState = AppState(
            setupChecklistState = SetupChecklistState(
                checklistItems = listOf(
                    expandedGroup,
                    collapsedGroup,
                ),
            ),
        )

        // Verify that the expanded group is expanded, and the other one is not
        assertTrue((appState.setupChecklistState!!.checklistItems[0] as ChecklistItem.Group).isExpanded)
        assertFalse((appState.setupChecklistState!!.checklistItems[1] as ChecklistItem.Group).isExpanded)

        val reducedState = SetupChecklistReducer.reduce(
            appState,
            AppAction.SetupChecklistAction.ChecklistItemClicked(collapsedGroup),
        )

        // Verify that the expanded group was collapsed, and the other one got expanded
        assertFalse((reducedState.setupChecklistState!!.checklistItems[0] as ChecklistItem.Group).isExpanded)
        assertTrue((reducedState.setupChecklistState!!.checklistItems[1] as ChecklistItem.Group).isExpanded)
    }

    @Test
    fun `WHEN a task item is clicked THEN the reduced state has the task marked completed`() {
        val task = ChecklistItem.Task(
            type = ChecklistItem.Task.Type.EXPLORE_EXTENSION,
            title = "task",
            icon = R.drawable.ic_addons_extensions,
            isCompleted = false,
        )

        val appState =
            AppState(setupChecklistState = SetupChecklistState(checklistItems = listOf(task)))
        val reducedState = SetupChecklistReducer.reduce(
            appState,
            AppAction.SetupChecklistAction.ChecklistItemClicked(task),
        )

        assertTrue((reducedState.setupChecklistState!!.checklistItems[0] as ChecklistItem.Task).isCompleted)
    }

    @Test
    fun `WHEN a task item within a group is clicked THEN the reduced state has only the clicked task marked completed`() {
        val taskToClick = ChecklistItem.Task(
            type = ChecklistItem.Task.Type.SET_AS_DEFAULT,
            title = "task1",
            icon = R.drawable.ic_addons_extensions,
            isCompleted = false,
        )
        val taskNoClick = ChecklistItem.Task(
            type = ChecklistItem.Task.Type.INSTALL_SEARCH_WIDGET,
            title = "task2",
            icon = R.drawable.ic_addons_extensions,
            isCompleted = false,
        )
        val group = ChecklistItem.Group(
            title = "group",
            tasks = listOf(taskToClick, taskNoClick),
            isExpanded = true,
        )

        val appState =
            AppState(setupChecklistState = SetupChecklistState(checklistItems = listOf(group)))
        val reducedState = SetupChecklistReducer.reduce(
            appState,
            AppAction.SetupChecklistAction.ChecklistItemClicked(taskToClick),
        )

        assertTrue((reducedState.setupChecklistState!!.checklistItems[0] as ChecklistItem.Group).tasks[0].isCompleted)
        assertFalse((reducedState.setupChecklistState!!.checklistItems[0] as ChecklistItem.Group).tasks[1].isCompleted)
    }
}
