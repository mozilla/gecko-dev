/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.setup.store

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.checklist.ChecklistItem
import org.mozilla.fenix.testDispatch

@RunWith(AndroidJUnit4::class)
class SetupChecklistStoreTest {

    private lateinit var store: SetupChecklistStore

    @Before
    fun setup() {
        store = SetupChecklistStore()
    }

    @Test
    fun `WHEN store is initialized THEN the initial state is correct`() {
        val initialState = SetupChecklistState()

        assertEquals(initialState.checklistItems, emptyList<ChecklistItem>())
    }

    @Test
    fun `WHEN the init action is dispatched THEN the state remains the same`() {
        val initialState = SetupChecklistState()

        store.testDispatch(SetupChecklistAction.Init)

        assertEquals(initialState, store.state)
    }

    @Test
    fun `WHEN the close action is dispatched THEN the state remains the same`() {
        val initialState = SetupChecklistState()

        store.testDispatch(SetupChecklistAction.Closed)

        assertEquals(initialState, store.state)
    }

    @Test
    fun `WHEN a group item is clicked THEN it changes its expanded state to the opposite `() {
        val initialState = true
        val group = ChecklistItem.Group(
            title = "A cool group",
            tasks = listOf(),
            isExpanded = initialState,
        )
        val store = buildStore(checklistItems = listOf(group))

        store.testDispatch(SetupChecklistAction.ChecklistItemClicked(group))

        val result = (store.state.checklistItems[0] as ChecklistItem.Group).isExpanded
        assertEquals(!initialState, result)
    }

    @Test
    fun `WHEN a task item is clicked THEN it changes its completed state to the opposite `() {
        val initialState = true
        val task = ChecklistItem.Task(
            type = ChecklistItem.Task.Type.EXPLORE_EXTENSION,
            title = "A cool task",
            icon = R.drawable.ic_addons_extensions,
            isCompleted = initialState,
        )
        val store = buildStore(checklistItems = listOf(task))

        store.testDispatch(SetupChecklistAction.ChecklistItemClicked(task))

        val result = (store.state.checklistItems[0] as ChecklistItem.Task).isCompleted
        assertEquals(!initialState, result)
    }

    @Test
    fun `WHEN a task item within a group is clicked THEN it changes its completed state to the opposite `() {
        val initialState = true
        val task = ChecklistItem.Task(
            type = ChecklistItem.Task.Type.EXPLORE_EXTENSION,
            title = "A cool task",
            icon = R.drawable.ic_addons_extensions,
            isCompleted = initialState,
        )
        val group = ChecklistItem.Group(
            title = "A cool group",
            tasks = listOf(task),
            isExpanded = true,
        )
        val store = buildStore(checklistItems = listOf(group))

        store.testDispatch(SetupChecklistAction.ChecklistItemClicked(task))

        val result = (store.state.checklistItems[0] as ChecklistItem.Group).tasks[0].isCompleted
        assertEquals(!initialState, result)
    }

    @Test
    fun `GIVEN multiple task items in a group WEHN a task item within a group is clicked THEN other tasks do not change their state `() {
        val initialState = true
        val clickedTask = ChecklistItem.Task(
            type = ChecklistItem.Task.Type.SET_AS_DEFAULT,
            title = "A cool task",
            icon = R.drawable.ic_addons_extensions,
            isCompleted = initialState,
        )
        val notClickedTask = ChecklistItem.Task(
            type = ChecklistItem.Task.Type.INSTALL_SEARCH_WIDGET,
            title = "A cooler task",
            icon = R.drawable.ic_addons_extensions,
            isCompleted = initialState,
        )
        val group = ChecklistItem.Group(
            title = "A cool group",
            tasks = listOf(clickedTask, notClickedTask),
            isExpanded = true,
        )
        val store = buildStore(checklistItems = listOf(group))

        store.testDispatch(SetupChecklistAction.ChecklistItemClicked(clickedTask))

        val result = (store.state.checklistItems[0] as ChecklistItem.Group).tasks[1].isCompleted
        assertEquals(initialState, result)
    }

    @Test
    fun `GIVEN an expanded group WHEN another group is clicked THEN the previously expanded group is collapsed`() {
        val expandedGroup = ChecklistItem.Group(
            title = "A cool group",
            tasks = listOf(
                ChecklistItem.Task(
                    type = ChecklistItem.Task.Type.SET_AS_DEFAULT,
                    title = "A cool task",
                    icon = R.drawable.ic_addons_extensions,
                    isCompleted = false,
                ),
            ),
            isExpanded = true,
        )
        val anotherGroup = ChecklistItem.Group(
            title = "A cooler group",
            tasks = listOf(
                ChecklistItem.Task(
                    type = ChecklistItem.Task.Type.INSTALL_SEARCH_WIDGET,
                    title = "A cooler task",
                    icon = R.drawable.ic_addons_extensions,
                    isCompleted = false,
                ),
            ),
            isExpanded = false,
        )
        val store = buildStore(checklistItems = listOf(expandedGroup, anotherGroup))

        // Verify that the expanded group is expanded, and the other one is not
        assertTrue((store.state.checklistItems[0] as ChecklistItem.Group).isExpanded)
        assertFalse((store.state.checklistItems[1] as ChecklistItem.Group).isExpanded)
        store.testDispatch(SetupChecklistAction.ChecklistItemClicked(anotherGroup))

        // Verify that the expanded group was collapsed, and the other one got expanded
        assertFalse((store.state.checklistItems[0] as ChecklistItem.Group).isExpanded)
        assertTrue((store.state.checklistItems[1] as ChecklistItem.Group).isExpanded)
    }

    private fun buildStore(checklistItems: List<ChecklistItem>) = SetupChecklistStore(
        initialState = SetupChecklistState(checklistItems = checklistItems),
    )
}
