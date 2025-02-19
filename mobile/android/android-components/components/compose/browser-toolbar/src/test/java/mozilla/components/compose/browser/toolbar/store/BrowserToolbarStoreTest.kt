/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar.store

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

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
}
