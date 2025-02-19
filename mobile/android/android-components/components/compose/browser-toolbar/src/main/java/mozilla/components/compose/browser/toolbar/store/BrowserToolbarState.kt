/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar.store

import mozilla.components.compose.browser.toolbar.concept.Action
import mozilla.components.lib.state.State

/**
 * The state of the browser toolbar.
 *
 * @property displayState Wrapper containing the toolbar display state.
 * @property editState Wrapper containing the toolbar edit state.
 * @property editMode Whether the toolbar is in "edit" or "display" mode.
 */
data class BrowserToolbarState(
    val displayState: DisplayState = DisplayState(),
    val editState: EditState = EditState(),
    val editMode: Boolean = false,
) : State

/**
 * Wrapper containing the toolbar display state.
 *
 * @property hint Text displayed in the toolbar when there's no URL to display
 * (no tab or empty URL).
 */
data class DisplayState(
    val hint: String = "",
) : State

/**
 * Wrapper containing the toolbar edit state.
 *
 * @property editText The text the user is editing in "edit" mode.
 * @property editActionsStart List of [Action]s to be displayed at the start of the URL of
 * the edit toolbar.
 * @property editActionsEnd List of [Action]s to be displayed at the end of the URL of
 * the edit toolbar.
 */
data class EditState(
    val editText: String? = null,
    val editActionsStart: List<Action> = emptyList(),
    val editActionsEnd: List<Action> = emptyList(),
) : State
