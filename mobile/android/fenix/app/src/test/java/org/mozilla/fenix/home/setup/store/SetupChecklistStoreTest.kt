/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.setup.store

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
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

        assertEquals(initialState.collection, SetupChecklistTaskCollection.THREE_TASKS)
        assertEquals(initialState.viewState, SetupChecklistViewState.FULL)
        assertFalse(initialState.defaultBrowserClicked)
        assertFalse(initialState.syncClicked)
        assertFalse(initialState.themeSelectionClicked)
        assertFalse(initialState.toolbarSelectionClicked)
        assertFalse(initialState.extensionsClicked)
        assertFalse(initialState.addSearchWidgetClicked)
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
    fun `WHEN the default browser clicked action is dispatched THEN defaultBrowserClicked is set to true`() {
        assertFalse(store.state.defaultBrowserClicked)

        store.testDispatch(SetupChecklistAction.DefaultBrowserClicked)

        val expectedState = SetupChecklistState(defaultBrowserClicked = true)
        assertEquals(expectedState, store.state)
    }

    @Test
    fun `WHEN the sync clicked action is dispatched THEN syncClicked is set to true`() {
        assertFalse(store.state.syncClicked)

        store.testDispatch(SetupChecklistAction.SyncClicked)

        val expectedState = SetupChecklistState(syncClicked = true)
        assertEquals(expectedState, store.state)
    }

    @Test
    fun `WHEN the theme selection clicked action is dispatched THEN themeSelectionClicked is set to true`() {
        assertFalse(store.state.themeSelectionClicked)

        store.testDispatch(SetupChecklistAction.ThemeSelectionClicked)

        val expectedState = SetupChecklistState(themeSelectionClicked = true)
        assertEquals(expectedState, store.state)
    }

    @Test
    fun `WHEN the toolbar selection clicked action is dispatched THEN toolbarSelectionClicked is set to true`() {
        assertFalse(store.state.toolbarSelectionClicked)

        store.testDispatch(SetupChecklistAction.ToolbarSelectionClicked)

        val expectedState = SetupChecklistState(toolbarSelectionClicked = true)
        assertEquals(expectedState, store.state)
    }

    @Test
    fun `WHEN the extensions clicked action is dispatched THEN extensionsClicked is set to true`() {
        assertFalse(store.state.extensionsClicked)

        store.testDispatch(SetupChecklistAction.ExtensionsClicked)

        val expectedState = SetupChecklistState(extensionsClicked = true)
        assertEquals(expectedState, store.state)
    }

    @Test
    fun `WHEN the add search widget clicked action is dispatched THEN addSearchWidgetClicked is set to true`() {
        assertFalse(store.state.addSearchWidgetClicked)

        store.testDispatch(SetupChecklistAction.AddSearchWidgetClicked)

        val expectedState = SetupChecklistState(addSearchWidgetClicked = true)
        assertEquals(expectedState, store.state)
    }

    @Test
    fun `WHEN the view state action is dispatched THEN the view state value is updated`() {
        assertEquals(store.state.viewState, SetupChecklistViewState.FULL)

        store.testDispatch(SetupChecklistAction.ViewState(SetupChecklistViewState.EXPANDED_FUNDAMENTALS))

        val expectedState =
            SetupChecklistState(viewState = SetupChecklistViewState.EXPANDED_FUNDAMENTALS)
        assertEquals(expectedState, store.state)
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

    private fun buildStore(checklistItems: List<ChecklistItem>) = SetupChecklistStore(
        initialState = SetupChecklistState(checklistItems = checklistItems),
    )
}
