/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar.store

import android.graphics.Color
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.compose.browser.toolbar.concept.Action.ActionButton
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import mozilla.components.ui.icons.R as iconsR

@RunWith(AndroidJUnit4::class)
class BrowserToolbarStoreTest {

    @get:Rule
    val coroutineTestRule = MainCoroutineRule()

    @Test
    fun `WHEN toggle edit mode action is dispatched THEN update the edit mode and states`() {
        val store = BrowserToolbarStore()
        val editMode = true

        assertFalse(store.state.editMode)

        store.dispatch(BrowserToolbarAction.ToggleEditMode(editMode = editMode))

        assertTrue(store.state.editMode)
        assertNull(store.state.editState.editText)
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
        val action1 = ActionButton(
            icon = iconsR.drawable.mozac_ic_search_24,
            contentDescription = null,
            tint = Color.BLACK,
            onClick = {},
        )
        val action2 = ActionButton(
            icon = iconsR.drawable.mozac_ic_forward_24,
            contentDescription = null,
            tint = Color.BLACK,
            onClick = {},
        )

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
        val action1 = ActionButton(
            icon = iconsR.drawable.mozac_ic_search_24,
            contentDescription = null,
            tint = Color.BLACK,
            onClick = {},
        )
        val action2 = ActionButton(
            icon = iconsR.drawable.mozac_ic_forward_24,
            contentDescription = null,
            tint = Color.BLACK,
            onClick = {},
        )

        assertEquals(0, store.state.editState.editActionsEnd.size)

        store.dispatch(BrowserEditToolbarAction.AddEditActionEnd(action = action1))

        assertEquals(1, store.state.editState.editActionsEnd.size)
        assertEquals(action1, store.state.editState.editActionsEnd.first())

        store.dispatch(BrowserEditToolbarAction.AddEditActionEnd(action = action2))

        assertEquals(2, store.state.editState.editActionsEnd.size)
        assertEquals(action1, store.state.editState.editActionsEnd.first())
        assertEquals(action2, store.state.editState.editActionsEnd.last())
    }
}
