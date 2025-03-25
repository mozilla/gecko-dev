/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar.store

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.compose.browser.toolbar.concept.Action.ActionButton
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.random.Random

@RunWith(AndroidJUnit4::class)
class BrowserToolbarStoreTest {

    @get:Rule
    val coroutineTestRule = MainCoroutineRule()

    @Test
    fun `WHEN toggle edit mode action is dispatched THEN update the mode and edit text states`() {
        val store = BrowserToolbarStore()
        val editMode = true

        assertEquals(Mode.DISPLAY, store.state.mode)

        store.dispatch(BrowserToolbarAction.ToggleEditMode(editMode = editMode))

        assertEquals(Mode.EDIT, store.state.mode)
        assertNull(store.state.editState.editText)
    }

    @Test
    fun `GIVEN browser actions already set WHEN updating them THEN replace all current ones with the newly provided list`() {
        val initialBrowserActions = listOf(fakeActionButton())
        val updatedListOfBrowserActions = listOf(fakeActionButton())
        val store = BrowserToolbarStore(
            initialState = BrowserToolbarState(
                displayState = DisplayState(
                    browserActions = initialBrowserActions,
                ),
            ),
        )

        store.dispatch(BrowserDisplayToolbarAction.UpdateBrowserActions(updatedListOfBrowserActions))

        assertEquals(store.state.displayState.browserActions, updatedListOfBrowserActions)
    }

    @Test
    fun `WHEN update edit text action is dispatched THEN update edit text state`() {
        val store = BrowserToolbarStore()
        val text = "Mozilla"

        assertNull(store.state.editState.editText)

        store.dispatch(BrowserEditToolbarAction.UpdateEditText(text = text))

        assertEquals(text, store.state.editState.editText)
    }

    @Test
    fun `WHEN add edit action start is dispatched THEN update edit actions start state`() {
        val store = BrowserToolbarStore()
        val action1 = fakeActionButton()
        val action2 = fakeActionButton()

        assertEquals(0, store.state.editState.editActionsStart.size)

        store.dispatch(BrowserEditToolbarAction.AddEditActionStart(action = action1))

        assertEquals(1, store.state.editState.editActionsStart.size)
        assertEquals(action1, store.state.editState.editActionsStart.first())

        store.dispatch(BrowserEditToolbarAction.AddEditActionStart(action = action2))

        assertEquals(2, store.state.editState.editActionsStart.size)
        assertEquals(action1, store.state.editState.editActionsStart.first())
        assertEquals(action2, store.state.editState.editActionsStart.last())
    }

    @Test
    fun `WHEN add edit action end is dispatched THEN update edit actions end state`() {
        val store = BrowserToolbarStore()
        val action1 = fakeActionButton()
        val action2 = fakeActionButton()

        assertEquals(0, store.state.editState.editActionsEnd.size)

        store.dispatch(BrowserEditToolbarAction.AddEditActionEnd(action = action1))

        assertEquals(1, store.state.editState.editActionsEnd.size)
        assertEquals(action1, store.state.editState.editActionsEnd.first())

        store.dispatch(BrowserEditToolbarAction.AddEditActionEnd(action = action2))

        assertEquals(2, store.state.editState.editActionsEnd.size)
        assertEquals(action1, store.state.editState.editActionsEnd.first())
        assertEquals(action2, store.state.editState.editActionsEnd.last())
    }

    @Test
    fun `WHEN add navigation action is dispatched THEN update display navigation actions state`() {
        val store = BrowserToolbarStore()
        val action1 = fakeActionButton()
        val action2 = fakeActionButton()

        assertEquals(0, store.state.displayState.navigationActions.size)

        store.dispatch(BrowserDisplayToolbarAction.AddNavigationAction(action = action1))

        assertEquals(1, store.state.displayState.navigationActions.size)
        assertEquals(action1, store.state.displayState.navigationActions.first())

        store.dispatch(BrowserDisplayToolbarAction.AddNavigationAction(action = action2))

        assertEquals(2, store.state.displayState.navigationActions.size)
        assertEquals(action1, store.state.displayState.navigationActions.first())
        assertEquals(action2, store.state.displayState.navigationActions.last())
    }

    @Test
    fun `WHEN add page action is dispatched THEN update display page actions state`() {
        val store = BrowserToolbarStore()
        val action1 = fakeActionButton()
        val action2 = fakeActionButton()

        assertEquals(0, store.state.displayState.pageActions.size)

        store.dispatch(BrowserDisplayToolbarAction.AddPageAction(action = action1))

        assertEquals(1, store.state.displayState.pageActions.size)
        assertEquals(action1, store.state.displayState.pageActions.first())

        store.dispatch(BrowserDisplayToolbarAction.AddPageAction(action = action2))

        assertEquals(2, store.state.displayState.pageActions.size)
        assertEquals(action1, store.state.displayState.pageActions.first())
        assertEquals(action2, store.state.displayState.pageActions.last())
    }

    @Test
    fun `WHEN add browser action is dispatched THEN update display browser actions state`() {
        val store = BrowserToolbarStore()
        val action1 = fakeActionButton()
        val action2 = fakeActionButton()

        assertEquals(0, store.state.displayState.browserActions.size)

        store.dispatch(BrowserDisplayToolbarAction.AddBrowserAction(action = action1))

        assertEquals(1, store.state.displayState.browserActions.size)
        assertEquals(action1, store.state.displayState.browserActions.first())

        store.dispatch(BrowserDisplayToolbarAction.AddBrowserAction(action = action2))

        assertEquals(2, store.state.displayState.browserActions.size)
        assertEquals(action1, store.state.displayState.browserActions.first())
        assertEquals(action2, store.state.displayState.browserActions.last())
    }

    private fun fakeActionButton() = ActionButton(
        icon = Random.nextInt(),
        contentDescription = null,
        tint = Random.nextInt(),
        onClick = {},
    )
}
