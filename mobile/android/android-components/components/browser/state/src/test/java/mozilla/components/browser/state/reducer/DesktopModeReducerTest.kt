/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.DefaultDesktopModeAction
import mozilla.components.browser.state.state.BrowserState
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class DesktopModeReducerTest {

    @Test
    fun `GIVEN desktop mode is disabled WHEN ToggleDesktopMode is dispatched THEN desktop mode is enabled`() {
        val state = BrowserState(desktopMode = false)
        val result = DesktopModeReducer.reduce(state = state, DefaultDesktopModeAction.ToggleDesktopMode)

        assertTrue(result.desktopMode)
    }

    @Test
    fun `GIVEN desktop mode is enabled WHEN ToggleDesktopMode is dispatched THEN desktop mode is disabled`() {
        val state = BrowserState(desktopMode = true)
        val result = DesktopModeReducer.reduce(state = state, DefaultDesktopModeAction.ToggleDesktopMode)

        assertFalse(result.desktopMode)
    }

    @Test
    fun `WHEN DesktopModeDefaultUpdated is dispatched THEN desktop mode is updated`() {
        val resultTrue = DesktopModeReducer.reduce(
            state = BrowserState(),
            action = DefaultDesktopModeAction.DesktopModeUpdated(newValue = true),
        )
        val resultFalse = DesktopModeReducer.reduce(
            state = BrowserState(),
            action = DefaultDesktopModeAction.DesktopModeUpdated(newValue = false),
        )

        assertTrue(resultTrue.desktopMode)
        assertFalse(resultFalse.desktopMode)
    }
}
