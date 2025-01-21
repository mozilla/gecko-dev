/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import android.util.JsonWriter
import mozilla.components.browser.state.action.UndoAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.UndoHistoryState
import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.browser.state.state.recover.TabState
import mozilla.components.concept.engine.EngineSessionState
import org.junit.Assert.assertEquals
import org.junit.Test

class UndoReducerTest {
    @Test
    fun `UpdateEngineStateForRecoverableTab will return a new undo history with updated session state`() {
        val browserState = BrowserState(
            undoHistory = UndoHistoryState(
                tag = "",
                tabs = listOf(
                    RecoverableTab(
                        engineSessionState = null,
                        state = TabState(id = "a", url = "https://www.mozilla.org", private = false),
                    ),
                ),
            ),
        )

        val engineSession = object : EngineSessionState {
            override fun writeTo(writer: JsonWriter) {
                // noop
            }
        }

        val result = UndoReducer.reduce(
            state = browserState,
            action = UndoAction.UpdateEngineStateForRecoverableTab(
                id = "a",
                engineSession,
            ),
        )

        assertEquals(engineSession, result.undoHistory.tabs[0].engineSessionState)
    }
}
